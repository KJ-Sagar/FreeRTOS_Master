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
# Services
# ==========================================================
SERVICE_HEARTBEAT = 0x1234
SERVICE_SENSOR    = 0x1001
SERVICE_ENGINE    = 0x1002

# ==========================================================
# Methods
# ==========================================================
METHOD_HEARTBEAT       = 0x0001
METHOD_SUBSCRIBE       = 0x0100
METHOD_UNSUBSCRIBE     = 0x0101
METHOD_GET_TEMPERATURE = 0x0001
METHOD_GET_RPM         = 0x0010

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

    session_id += 1
    return hdr + payload

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
            client_id,
            sess,
            proto,
            iface,
            msg_type,
            ret
        ) = struct.unpack(HEADER_FMT, hdr_raw)

        payload_len = length - 8
        payload = recv_exact(sock, payload_len) if payload_len > 0 else b""

        # ---------------- NOTIFICATION ----------------
        if msg_type == SOMEIP_MSG_NOTIFICATION:
            if service_id == SERVICE_HEARTBEAT and len(payload) == 4:
                alive = struct.unpack("!I", payload)[0]
                print(f"[NOTIFY] Heartbeat alive = {alive}")
            else:
                print("[NOTIFY] Unknown or malformed notification")
            continue

        # ---------------- ERROR ----------------
        if msg_type == SOMEIP_MSG_ERROR or ret != 0:
            print(f"[ERROR] service=0x{service_id:04X} method=0x{method_id:04X}")
            continue

        # ---------------- RESPONSE ----------------
        if service_id == SERVICE_HEARTBEAT:
            if len(payload) == 4:
                alive = struct.unpack("!I", payload)[0]
                print(f"[RESP] Heartbeat = {alive}")
            else:
                print("[RESP] Heartbeat ACK")

        elif service_id == SERVICE_SENSOR:
            if len(payload) == 4:
                temp = struct.unpack("!i", payload)[0]
                print(f"[RESP] Temperature = {temp}")
            else:
                print("[RESP] Sensor ACK")

        elif service_id == SERVICE_ENGINE:
            if len(payload) == 2:
                rpm = struct.unpack("!H", payload)[0]
                print(f"[RESP] RPM = {rpm}")
            else:
                print("[RESP] Engine ACK")

# ==========================================================
# Main
# ==========================================================
print(f"[CLIENT] Connecting to {SERVER_IP}:{SERVER_PORT}")
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((SERVER_IP, SERVER_PORT))
print("[CLIENT] Connected")

threading.Thread(target=receiver, args=(sock,), daemon=True).start()

# ---------------- Subscribe ----------------
print("[TX] Subscribe to heartbeat notifications")
sock.sendall(build_request(SERVICE_HEARTBEAT, METHOD_SUBSCRIBE))
time.sleep(1)

# ---------------- Periodic requests ----------------
try:
    while running:
        sock.sendall(build_request(SERVICE_HEARTBEAT, METHOD_HEARTBEAT))
        time.sleep(2)

        sock.sendall(build_request(SERVICE_SENSOR, METHOD_GET_TEMPERATURE))
        time.sleep(2)

        sock.sendall(build_request(SERVICE_ENGINE, METHOD_GET_RPM))
        time.sleep(5)

except KeyboardInterrupt:
    print("\n[CLIENT] Unsubscribe and exit")

finally:
    try:
        sock.sendall(build_request(SERVICE_HEARTBEAT, METHOD_UNSUBSCRIBE))
        time.sleep(0.5)
    except OSError:
        pass

    running = False
    sock.close()
    print("[CLIENT] Disconnected")
