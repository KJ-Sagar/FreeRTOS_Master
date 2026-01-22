#!/usr/bin/env python3
import socket
import struct
import threading
import time

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

    session_id = (session_id + 1) & 0xFFFF
    return hdr + payload


# ==========================================================
# Service Discovery (Unicast)
# ==========================================================
def sd_find_services():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(2.0)

    print("[SD] Sending FindService (unicast)")

    try:
        sock.sendto(b"\x00", (SERVER_IP, SD_UDP_PORT))
        data, addr = sock.recvfrom(256)
    except socket.timeout:
        print("[SD] No Service Discovery response")
        sock.close()
        return

    print(f"[SD] Offer received from {addr[0]}")
    print("[SD] Services offered:")

    for i in range(0, len(data), 2):
        sid = struct.unpack_from("!H", data, i)[0]
        print(f"  Service ID: 0x{sid:04X}")

    sock.close()


# ==========================================================
# Receiver thread
# ==========================================================
def receiver(sock):
    global running

    while running:
        hdr_raw = recv_exact(sock, HEADER_SIZE)
        if hdr_raw is None:
            print("[RX] Server disconnected")
            running = False
            return

        (
            service_id,
            method_id,
            length,
            _client,
            _session,
            _proto,
            _iface,
            msg_type,
            ret
        ) = struct.unpack(HEADER_FMT, hdr_raw)

        payload_len = length - 8
        payload = recv_exact(sock, payload_len) if payload_len > 0 else b""

        # ---------- NOTIFICATION ----------
        if msg_type == SOMEIP_MSG_NOTIFICATION:
            if service_id == SERVICE_HEARTBEAT and len(payload) == 4:
                alive = struct.unpack("!I", payload)[0]
                print(f"[NOTIFY] Heartbeat alive = {alive}")
            else:
                print("[NOTIFY] Unknown notification")
            continue

        # ---------- ERROR ----------
        if msg_type == SOMEIP_MSG_ERROR or ret != 0:
            print(f"[ERROR] service=0x{service_id:04X} method=0x{method_id:04X}")
            continue

        # ---------- RESPONSE ----------
        print("[RESP] ACK from server")


# ==========================================================
# Main
# ==========================================================
time.sleep(2)

# ---- Phase 1: Service Discovery ----
sd_find_services()
time.sleep(1)

# ---- Phase 2: TCP SOME/IP ----
print(f"[CLIENT] Connecting to {SERVER_IP}:{SERVER_PORT}")
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((SERVER_IP, SERVER_PORT))
print("[CLIENT] Connected")

threading.Thread(
    target=receiver,
    args=(sock,),
    daemon=True
).start()

# ---- SUBSCRIBE ----
subscribe_msg = build_request(SERVICE_HEARTBEAT, METHOD_SUBSCRIBE)
print("[TX] Subscribe to heartbeat")
sock.sendall(subscribe_msg)

try:
    while running:
        time.sleep(5)
        # Optional polling request (can be commented out)
        sock.sendall(build_request(SERVICE_HEARTBEAT, METHOD_HEARTBEAT))

except KeyboardInterrupt:
    print("\n[CLIENT] Exit")

finally:
    try:
        sock.sendall(build_request(SERVICE_HEARTBEAT, METHOD_UNSUBSCRIBE))
        time.sleep(0.5)
    except OSError:
        pass

    running = False
    sock.close()
    print("[CLIENT] Disconnected")
