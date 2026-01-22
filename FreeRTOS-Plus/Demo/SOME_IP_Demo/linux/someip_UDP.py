#!/usr/bin/env python3
import socket
import struct
import threading
import time
import sys
from datetime import datetime

# ==========================================================
# Logging helpers
# ==========================================================
def ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]

def log(msg):
    print(f"[{ts()}] {msg}", flush=True)

def hexdump(data):
    return " ".join(f"{b:02X}" for b in data)

# ==========================================================
# Network
# ==========================================================
SERVER_IP   = "10.0.0.2"
SERVER_PORT = 30509
SD_UDP_PORT = 30490

# ==========================================================
# SOME/IP constants
# ==========================================================
SOMEIP_MSG_REQUEST      = 0x00
SOMEIP_MSG_NOTIFICATION = 0x02
SOMEIP_MSG_RESPONSE     = 0x80
SOMEIP_MSG_ERROR        = 0x81

SOMEIP_PROTOCOL_VERSION  = 0x01
SOMEIP_INTERFACE_VERSION = 0x01

CLIENT_ID = 0x0001

# ==========================================================
# Services / Methods
# ==========================================================
SERVICE_HEARTBEAT = 0x1234
SERVICE_SENSOR    = 0x1001
SERVICE_ENGINE    = 0x1002

METHOD_HEARTBEAT   = 0x0001
METHOD_SUBSCRIBE   = 0x0100
METHOD_UNSUBSCRIBE = 0x0101

# ==========================================================
# SOME/IP header
# ==========================================================
HEADER_FMT  = "!HHIHHBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

# ==========================================================
# Global state
# ==========================================================
session_id = 1
running = True

# ==========================================================
# Helpers
# ==========================================================
def recv_exact(sock, length):
    data = b""
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            return None
        data += chunk
    return data


def build_request(service_id, method_id, payload=b""):
    global session_id

    length = 8 + len(payload)

    hdr = struct.pack(
        HEADER_FMT,
        service_id,
        method_id,
        length,
        CLIENT_ID,
        session_id,
        SOMEIP_PROTOCOL_VERSION,
        SOMEIP_INTERFACE_VERSION,
        SOMEIP_MSG_REQUEST,
        0x00
    )

    log(f"TX BUILD: SID=0x{service_id:04X} "
        f"MID=0x{method_id:04X} "
        f"SESSION={session_id} "
        f"LEN={length}")

    log(f"TX HEADER RAW: {hexdump(hdr)}")

    session_id = (session_id + 1) & 0xFFFF
    return hdr + payload


# ==========================================================
# Service Discovery (Unicast)
# ==========================================================
def sd_find_services():
    log("SD: Creating UDP socket")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)

    log("SD: Sending FindService (unicast)")
    try:
        sock.sendto(b"\x00", (SERVER_IP, SD_UDP_PORT))
        data, addr = sock.recvfrom(256)
    except socket.timeout:
        log("SD: No Service Discovery response")
        sock.close()
        return

    log(f"SD: Offer received from {addr[0]}")
    log("SD: Services offered:")

    for i in range(0, len(data), 2):
        sid = struct.unpack_from("!H", data, i)[0]
        log(f"  -> Service ID: 0x{sid:04X}")

    sock.close()
    log("SD: Socket closed")


# ==========================================================
# Receiver thread
# ==========================================================
def receiver(sock):
    global running

    log("RX THREAD: Started")

    while running:
        hdr_raw = recv_exact(sock, HEADER_SIZE)
        if hdr_raw is None:
            log("RX THREAD: recv_exact returned None (server closed connection)")
            running = False
            break

        log(f"RX HEADER RAW: {hexdump(hdr_raw)}")

        (
            service_id,
            method_id,
            length,
            client_id,
            session,
            proto,
            iface,
            msg_type,
            ret
        ) = struct.unpack(HEADER_FMT, hdr_raw)

        log(f"RX HEADER: "
            f"SID=0x{service_id:04X} "
            f"MID=0x{method_id:04X} "
            f"SESSION={session} "
            f"LEN={length} "
            f"TYPE=0x{msg_type:02X} "
            f"RET=0x{ret:02X}")

        if length < 8:
            log("RX ERROR: Invalid SOME/IP length")
            continue

        payload_len = length - 8
        payload = recv_exact(sock, payload_len) if payload_len > 0 else b""

        if payload_len > 0:
            log(f"RX PAYLOAD RAW: {hexdump(payload)}")

        # ---------- NOTIFICATION ----------
        if msg_type == SOMEIP_MSG_NOTIFICATION:
            if service_id == SERVICE_HEARTBEAT and len(payload) == 4:
                alive = struct.unpack("!I", payload)[0]
                log(f"RX NOTIFY: Heartbeat alive = {alive}")
            else:
                log("RX NOTIFY: Unknown notification")
            continue

        # ---------- ERROR ----------
        if msg_type == SOMEIP_MSG_ERROR or ret != 0:
            log(f"RX ERROR: service=0x{service_id:04X} "
                f"method=0x{method_id:04X}")
            continue

        # ---------- RESPONSE ----------
        log("RX RESPONSE: ACK received")

    log("RX THREAD: Exiting")

# ==========================================================
# Main
# ==========================================================
log("CLIENT: Starting")
time.sleep(2)

# ---- Phase 1: Service Discovery ----
sd_find_services()
time.sleep(1)

# ---- Phase 2: TCP SOME/IP ----
log(f"CLIENT: Connecting to {SERVER_IP}:{SERVER_PORT}")
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((SERVER_IP, SERVER_PORT))
log("CLIENT: TCP connected")

rx_thread = threading.Thread(
    target=receiver,
    args=(sock,),
    daemon=True
)
rx_thread.start()

log("CLIENT: Sending SUBSCRIBE")
sock.sendall(build_request(SERVICE_HEARTBEAT, METHOD_SUBSCRIBE))

try:
    while running:
        time.sleep(5)
        log("CLIENT: Sending heartbeat REQUEST")
        sock.sendall(build_request(SERVICE_HEARTBEAT, METHOD_HEARTBEAT))

except KeyboardInterrupt:
    log("CLIENT: KeyboardInterrupt received")

finally:
    log("CLIENT: Sending UNSUBSCRIBE")
    try:
        sock.sendall(build_request(SERVICE_HEARTBEAT, METHOD_UNSUBSCRIBE))
        time.sleep(0.5)
    except OSError as e:
        log(f"CLIENT: Socket error during unsubscribe: {e}")

    running = False
    sock.close()
    log("CLIENT: Socket closed")
    log("CLIENT: Exit")
