import socket
import struct
import time

# ------------------------------------
# Configuration
# ------------------------------------
SERVER_IP   = "10.0.0.2"   # FreeRTOS QEMU
SERVER_PORT = 30509

SERVICE_ID = 0x1234
METHOD_ID  = 0x0001
CLIENT_ID  = 0x0001
SESSION_ID = 0x0001

SOMEIP_PROTOCOL_VERSION  = 0x01
SOMEIP_INTERFACE_VERSION = 0x01

SOMEIP_MSG_REQUEST  = 0x00
SOMEIP_MSG_RESPONSE = 0x80

REQUEST_COUNT = 5
REQUEST_INTERVAL_SEC = 1

# ------------------------------------
# Build SOME/IP request header
# ------------------------------------
def build_request():
    return struct.pack(
        "!HHIHHBBBB",
        SERVICE_ID,
        METHOD_ID,
        0,                  # length (no payload)
        CLIENT_ID,
        SESSION_ID,
        SOMEIP_PROTOCOL_VERSION,
        SOMEIP_INTERFACE_VERSION,
        SOMEIP_MSG_REQUEST,
        0x00                # return code
    )

# ------------------------------------
# Connect once
# ------------------------------------
print(f"SOMEIP client: connecting to {SERVER_IP}:{SERVER_PORT}")

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((SERVER_IP, SERVER_PORT))

print("Connected (persistent session)\n")

# ------------------------------------
# Send multiple requests
# ------------------------------------
methods = [
    (0x0001, "Temperature"),
    (0x0002, "RPM"),
    (0x0003, "Status"),
]

for method_id, name in methods:
    print(f"\nRequesting {name}")

    header = struct.pack(
        "!HHIHHBBBB",
        SERVICE_ID,
        method_id,
        0,
        CLIENT_ID,
        SESSION_ID,
        SOMEIP_PROTOCOL_VERSION,
        SOMEIP_INTERFACE_VERSION,
        SOMEIP_MSG_REQUEST,
        0x00
    )

    sock.sendall(header)

    resp_hdr = sock.recv(16)
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
    ) = struct.unpack("!HHIHHBBBB", resp_hdr)
    print(
        f"  Response Header:"
        f" service=0x{svc_id:04X},"
        f" method=0x{mtd_id:04X},"
        f" length={length},"
        f" msg_type=0x{msg_type:02X}"
    )

    if ret_code != 0:
        print("  Error: unknown method")
        continue

    payload = sock.recv(length)

    if method_id == 0x0001:
        temp = struct.unpack("!i", payload)[0]
        print(f"  Payload: Temperature = {temp / 10.0:.1f} °C")

    elif method_id == 0x0002:
        rpm = struct.unpack("!H", payload)[0]
        print(f"  Payload: RPM = {rpm}")
   
    elif method_id == 0x0003:
        status = payload[0]
        print(f"  Payload: Status = {status}")

# ------------------------------------
# Close session
# ------------------------------------
print("\nClosing connection")
sock.close()
print("Client done")
