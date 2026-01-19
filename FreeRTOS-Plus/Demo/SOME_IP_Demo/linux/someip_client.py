#!/usr/bin/env python3
import socket
import struct
import time

# ------------------------------------
# Network Configuration
# ------------------------------------
SERVER_IP   = "10.0.0.2"
SERVER_PORT = 30509

# ------------------------------------
# SOME/IP Constants (MUST MATCH SERVER)
# ------------------------------------
SOMEIP_MSG_REQUEST  = 0x00
SOMEIP_MSG_RESPONSE = 0x80
SOMEIP_MSG_ERROR    = 0x81

SOMEIP_PROTOCOL_VERSION  = 0x01
SOMEIP_INTERFACE_VERSION = 0x01

CLIENT_ID  = 0x0001
SESSION_ID = 0x0001

# ------------------------------------
# Services
# ------------------------------------
SERVICE_SENSOR = 0x1001
SERVICE_ENGINE = 0x1002
SERVICE_SYSTEM = 0x1003

# ------------------------------------
# Methods
# ------------------------------------
METHOD_GET_TEMPERATURE = 0x0001
METHOD_GET_HUMIDITY    = 0x0002

METHOD_GET_RPM         = 0x0010
METHOD_GET_TORQUE      = 0x0011

METHOD_GET_STATUS      = 0x0020
METHOD_GET_UPTIME      = 0x0021

# ------------------------------------
# SOME/IP header format
# ------------------------------------
HEADER_FMT  = "!HHIHHBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

# ------------------------------------
# Build SOME/IP request
# ------------------------------------
def build_request(service_id, method_id):
    return struct.pack(
        HEADER_FMT,
        service_id,
        method_id,
        0,                  # payload length
        CLIENT_ID,
        SESSION_ID,
        SOMEIP_PROTOCOL_VERSION,
        SOMEIP_INTERFACE_VERSION,
        SOMEIP_MSG_REQUEST,
        0x00                # return code (unused in request)
    )

# ------------------------------------
# Receive exact number of bytes
# ------------------------------------
def recv_exact(sock, length):
    data = b""
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            raise ConnectionError("Socket closed")
        data += chunk
    return data

# ------------------------------------
# Connect
# ------------------------------------
print(f"SOME/IP client: connecting to {SERVER_IP}:{SERVER_PORT}")
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((SERVER_IP, SERVER_PORT))
print("Connected\n")

# ------------------------------------
# Requests to perform
# ------------------------------------
requests = [
    (SERVICE_SENSOR, METHOD_GET_TEMPERATURE, "Temperature"),
    (SERVICE_SENSOR, METHOD_GET_HUMIDITY,    "Humidity"),
    (SERVICE_ENGINE, METHOD_GET_RPM,          "RPM"),
    (SERVICE_ENGINE, METHOD_GET_TORQUE,       "Torque"),
    (SERVICE_SYSTEM, METHOD_GET_STATUS,       "Status"),
    (SERVICE_SYSTEM, METHOD_GET_UPTIME,       "Uptime"),
]

# ------------------------------------
# Send requests
# ------------------------------------
for service_id, method_id, name in requests:
    print(f"Requesting {name}")

    req = build_request(service_id, method_id)
    sock.sendall(req)

    # --- Receive header ---
    resp_hdr = recv_exact(sock, HEADER_SIZE)

    (
        svc_id,
        mtd_id,
        length,
        client_id,
        session_id,
        proto_ver,
        iface_ver,
        msg_type,
        ret_code
    ) = struct.unpack(HEADER_FMT, resp_hdr)

    print(
        f"  Response Header:"
        f" service=0x{svc_id:04X},"
        f" method=0x{mtd_id:04X},"
        f" length={length},"
        f" msg_type=0x{msg_type:02X},"
        f" ret=0x{ret_code:02X}"
    )

    # --- Handle errors ---
    if msg_type == SOMEIP_MSG_ERROR or ret_code != 0:
        print("  Error response from server\n")
        continue

    # --- Receive payload ---
    payload = recv_exact(sock, length) if length > 0 else b""

    # --- Decode payload ---
    if method_id == METHOD_GET_TEMPERATURE and length == 4:
        temp = struct.unpack("!i", payload)[0]
        print(f"  Payload: Temperature = {temp / 10.0:.1f} °C")

    elif method_id == METHOD_GET_HUMIDITY and length == 1:
        humidity = payload[0]
        print(f"  Payload: Humidity = {humidity} %")

    elif method_id == METHOD_GET_RPM and length == 2:
        rpm = struct.unpack("!H", payload)[0]
        print(f"  Payload: RPM = {rpm}")

    elif method_id == METHOD_GET_TORQUE and length == 2:
        torque = struct.unpack("!H", payload)[0]
        print(f"  Payload: Torque = {torque} Nm")

    elif method_id == METHOD_GET_STATUS and length == 1:
        status = payload[0]
        print(f"  Payload: Status = {status}")

    elif method_id == METHOD_GET_UPTIME and length == 4:
        uptime = struct.unpack("!I", payload)[0]
        print(f"  Payload: Uptime = {uptime} ticks")

    else:
        print(f"  Unknown or malformed payload (len={length})")

    print()
    time.sleep(1)

# ------------------------------------
# Close connection
# ------------------------------------
print("Closing connection")
sock.close()
print("Client done")
