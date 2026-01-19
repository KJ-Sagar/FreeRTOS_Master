#!/usr/bin/env python3
import socket
import struct
import threading
import time
import sys

# ==========================================================
# Network Configuration
# ==========================================================
SERVER_IP   = "10.0.0.2"
SERVER_PORT = 30509

# ==========================================================
# SOME/IP Constants
# ==========================================================
SOMEIP_MSG_REQUEST      = 0x00
SOMEIP_MSG_NOTIFICATION = 0x02
SOMEIP_MSG_RESPONSE     = 0x80
SOMEIP_MSG_ERROR        = 0x81

SOMEIP_PROTOCOL_VERSION  = 0x01
SOMEIP_INTERFACE_VERSION = 0x01

CLIENT_ID  = 0x0001
SESSION_ID = 0x0001

# ==========================================================
# Services
# ==========================================================
SERVICE_SENSOR = 0x1001
SERVICE_ENGINE = 0x1002
SERVICE_SYSTEM = 0x1003
SERVICE_SD     = 0xFFFF

# ==========================================================
# Methods
# ==========================================================
METHOD_GET_TEMPERATURE = 0x0001
METHOD_GET_HUMIDITY    = 0x0002
METHOD_SUBSCRIBE       = 0x0100
METHOD_UNSUBSCRIBE     = 0x0101

METHOD_GET_RPM         = 0x0010
METHOD_GET_TORQUE      = 0x0011

METHOD_GET_STATUS      = 0x0020
METHOD_GET_UPTIME      = 0x0021

METHOD_SD_OFFER        = 0x0001

# ==========================================================
# SOME/IP header
# ==========================================================
HEADER_FMT  = "!HHIHHBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

# ==========================================================
# Global state
# ==========================================================
running = True

# ==========================================================
# TCP helpers
# ==========================================================
def recv_exact(sock, length):
    data = b""
    while len(data) < length:
        try:
            chunk = sock.recv(length - len(data))
        except OSError:
            return None
        if not chunk:
            return None
        data += chunk
    return data

def build_request(service_id, method_id):
    return struct.pack(
        HEADER_FMT,
        service_id,
        method_id,
        0,
        CLIENT_ID,
        SESSION_ID,
        SOMEIP_PROTOCOL_VERSION,
        SOMEIP_INTERFACE_VERSION,
        SOMEIP_MSG_REQUEST,
        0x00
    )

# ==========================================================
# Receiver thread
# ==========================================================
def receiver_thread(sock):
    global running

    while running:
        hdr = recv_exact(sock, HEADER_SIZE)
        if hdr is None:
            print("\n[RX] Server closed connection")
            running = False
            return

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

        payload = recv_exact(sock, length) if length > 0 else b""

        # ---------------- NOTIFICATION ----------------
        if msg_type == SOMEIP_MSG_NOTIFICATION:
            if service_id == SERVICE_SENSOR and method_id == METHOD_GET_TEMPERATURE:
                if len(payload) != 4:
                    print("[NOTIFY] Invalid temperature payload length:", len(payload))
                    continue

                temp = struct.unpack("!i", payload)[0] / 10.0
                print(f"[NOTIFY] Temperature = {temp:.1f} °C")
            else:
                print(f"[NOTIFY] service=0x{service_id:04X} method=0x{method_id:04X}")
            continue

        # ---------------- ERROR ----------------
        if msg_type == SOMEIP_MSG_ERROR or ret_code != 0:
            print(f"[ERROR] service=0x{service_id:04X} method=0x{method_id:04X}")
            continue

        # ---------------- RESPONSE ----------------
        if service_id == SERVICE_SD:
            if length == 0:
                print("[SD] No services advertised")
            else:
                services = struct.unpack(f"!{length//2}H", payload)
                services = [f"0x{s:04X}" for s in services]
                print(f"[SD] Services offered: {services}")

        elif service_id == SERVICE_SENSOR:
            if method_id == METHOD_GET_TEMPERATURE and len(payload) == 4:
                temp = struct.unpack("!i", payload)[0] / 10.0
                print(f"[RESP] Temperature = {temp:.1f} °C")
            elif method_id == METHOD_GET_HUMIDITY and len(payload) == 1:
                print(f"[RESP] Humidity = {payload[0]} %")

        elif service_id == SERVICE_ENGINE:
            if method_id == METHOD_GET_RPM and len(payload) == 2:
                rpm = struct.unpack("!H", payload)[0]
                print(f"[RESP] RPM = {rpm}")
            elif method_id == METHOD_GET_TORQUE and len(payload) == 2:
                tq = struct.unpack("!H", payload)[0]
                print(f"[RESP] Torque = {tq} Nm")

        elif service_id == SERVICE_SYSTEM:
            if method_id == METHOD_GET_STATUS and len(payload) == 1:
                print(f"[RESP] Status = {payload[0]}")
            elif method_id == METHOD_GET_UPTIME and len(payload) == 4:
                uptime = struct.unpack("!I", payload)[0]
                print(f"[RESP] Uptime = {uptime} ticks")

# ==========================================================
# Main
# ==========================================================
print(f"SOME/IP client: connecting to {SERVER_IP}:{SERVER_PORT}")
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((SERVER_IP, SERVER_PORT))
print("Connected\n")

threading.Thread(target=receiver_thread, args=(sock,), daemon=True).start()

# ---------------- Service Discovery ----------------
print("[TX] Service Discovery request")
sock.sendall(build_request(SERVICE_SD, METHOD_SD_OFFER))
time.sleep(1)

# ---------------- Subscribe ----------------
print("[TX] Subscribe to temperature notifications")
sock.sendall(build_request(SERVICE_SENSOR, METHOD_SUBSCRIBE))
time.sleep(1)

# ---------------- Periodic requests ----------------
try:
    while running:
        sock.sendall(build_request(SERVICE_SENSOR, METHOD_GET_TEMPERATURE))
        time.sleep(5)
        sock.sendall(build_request(SERVICE_ENGINE, METHOD_GET_RPM))
        time.sleep(5)

except KeyboardInterrupt:
    print("\n[TX] Unsubscribe and exit")

finally:
    running = False
    try:
        sock.sendall(build_request(SERVICE_SENSOR, METHOD_UNSUBSCRIBE))
        time.sleep(0.5)
    except OSError:
        pass

    sock.close()
    print("Disconnected")
