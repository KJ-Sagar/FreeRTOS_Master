#!/usr/bin/env python3
import socket
import struct
import sys

# ==========================================================
# Network Configuration
# ==========================================================
SERVER_IP   = "10.0.0.2"
SERVER_PORT = 30509

# ==========================================================
# SOME/IP Constants (MUST MATCH SERVER)
# ==========================================================
SOMEIP_MSG_REQUEST  = 0x00
SOMEIP_MSG_RESPONSE = 0x80
SOMEIP_MSG_ERROR    = 0x81

SOMEIP_PROTOCOL_VERSION  = 0x01
SOMEIP_INTERFACE_VERSION = 0x01

CLIENT_ID  = 0x0001
SESSION_ID = 0x0001

# ==========================================================
# Heartbeat Service (ONLY SERVICE IMPLEMENTED)
# ==========================================================
SERVICE_HEARTBEAT = 0x1234
METHOD_HEARTBEAT  = 0x0001

# ==========================================================
# SOME/IP Header
# ==========================================================
# ! = network byte order
# service_id, method_id, length, client_id, session_id,
# proto_ver, iface_ver, msg_type, return_code
HEADER_FMT  = "!HHIHHBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

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

def build_heartbeat_request():
    payload_len = 0
    length = 8  # payload_len (0) + SOMEIP_HEADER_PAYLOAD_OFFSET (8)

    return struct.pack(
        HEADER_FMT,
        SERVICE_HEARTBEAT,
        METHOD_HEARTBEAT,
        length,
        CLIENT_ID,
        SESSION_ID,
        SOMEIP_PROTOCOL_VERSION,
        SOMEIP_INTERFACE_VERSION,
        SOMEIP_MSG_REQUEST,
        0x00
    )

# ==========================================================
# Main
# ==========================================================
print(f"[CLIENT] Connecting to {SERVER_IP}:{SERVER_PORT}")
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((SERVER_IP, SERVER_PORT))
print("[CLIENT] Connected")

print("[TX] Heartbeat request")
sock.sendall(build_heartbeat_request())

hdr = recv_exact(sock, HEADER_SIZE)
if hdr is None:
    print("[RX] No response (server closed connection)")
    sys.exit(1)

(
    service_id,
    method_id,
    length,
    client_id,
    session_id,
    proto_ver,
    iface_ver,
    msg_type,
    ret_code
) = struct.unpack(HEADER_FMT, hdr)

payload_len = length - 8
payload = recv_exact(sock, payload_len) if payload_len > 0 else b""

print(f"[RX] service=0x{service_id:04X} method=0x{method_id:04X}")

if msg_type == SOMEIP_MSG_ERROR or ret_code != 0:
    print("[RX] ERROR from server")
    sys.exit(1)

if payload_len != 4:
    print(f"[RX] Invalid payload length: {payload_len}")
    sys.exit(1)

alive = struct.unpack("!I", payload)[0]
print(f"[RX] Heartbeat alive = {alive}")
print("[CLIENT] Heartbeat successful")
sock.close()
print("[CLIENT] Disconnected")
