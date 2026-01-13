import socket
import struct

# ------------------------------------
# Configuration
# ------------------------------------
SERVER_IP = "10.0.0.2"   # QEMU FreeRTOS IP
SERVER_PORT = 30509

SERVICE_ID = 0x1234
METHOD_ID  = 0x0001
CLIENT_ID  = 0x0001
SESSION_ID = 0x0001

SOMEIP_PROTOCOL_VERSION  = 0x01
SOMEIP_INTERFACE_VERSION = 0x01

SOMEIP_MSG_REQUEST  = 0x00
SOMEIP_MSG_RESPONSE = 0x80

# ------------------------------------
# Build SOME/IP request header
# ------------------------------------
someip_header = struct.pack(
    "!HHIHHBBBB",
    SERVICE_ID,
    METHOD_ID,
    0,                 # length (no payload)
    CLIENT_ID,
    SESSION_ID,
    SOMEIP_PROTOCOL_VERSION,
    SOMEIP_INTERFACE_VERSION,
    SOMEIP_MSG_REQUEST,
    0x00               # return code
)

print(f"SOMEIP client: connecting to {SERVER_IP}:{SERVER_PORT}")

# ------------------------------------
# TCP connect
# ------------------------------------
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((SERVER_IP, SERVER_PORT))

print("Connected, sending SOME/IP request")

sock.sendall(someip_header)

# ------------------------------------
# Receive response header
# ------------------------------------
resp_hdr = sock.recv(16)
if len(resp_hdr) < 16:
    raise RuntimeError("Incomplete SOME/IP header received")

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

print("SOME/IP response header:")
print(f"  Service ID : 0x{svc_id:04x}")
print(f"  Method ID  : 0x{mtd_id:04x}")
print(f"  Length     : {length}")
print(f"  Msg Type   : 0x{msg_type:02x}")

# ------------------------------------
# Receive payload (temperature)
# ------------------------------------
if length == 4:
    payload = sock.recv(4)
    temperature = struct.unpack("!i", payload)[0]
    print(f"Temperature: {temperature / 10.0:.1f} °C")

sock.close()
print("Client done")
