#!/usr/bin/env python3
"""
SOME/IP Python Client - Phase 4 Enhanced
Features:
- Persistent TCP connection
- Event group subscription
- TTL specification
- Clean shutdown with ACK wait
- Multi-client capable
"""
import socket
import struct
import threading
import time
import sys
from datetime import datetime

# ==========================================================
# Logging
# ==========================================================
def ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]

def log(msg):
    print(f"[{ts()}] {msg}", flush=True)

def hexdump(data):
    return " ".join(f"{b:02X}" for b in data)

# ==========================================================
# Network Configuration
# ==========================================================
SERVER_IP   = "10.0.0.2"
SERVER_PORT = 30509
SD_UDP_PORT = 30490

# ==========================================================
# SOME/IP Constants
# ==========================================================
SOMEIP_MSG_REQUEST      = 0x00
SOMEIP_MSG_NOTIFICATION = 0x02
SOMEIP_MSG_RESPONSE     = 0x80
SOMEIP_MSG_ERROR        = 0x81

SOMEIP_PROTOCOL_VERSION  = 0x01
SOMEIP_INTERFACE_VERSION = 0x01

CLIENT_ID = 0x0001

# ==========================================================
# Services / Methods / Event Groups
# ==========================================================
SERVICE_HEARTBEAT = 0x1234
SERVICE_SENSOR    = 0x1001
SERVICE_ENGINE    = 0x1002

METHOD_HEARTBEAT   = 0x0001
METHOD_SUBSCRIBE   = 0x0100
METHOD_UNSUBSCRIBE = 0x0101

EVENTGROUP_STATUS = 0x0001  # Heartbeat status event group

# ==========================================================
# SOME/IP Header
# ==========================================================
HEADER_FMT  = "!HHIHHBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

# ==========================================================
# Global State
# ==========================================================
session_id = 1
running = True

# ==========================================================
# Helpers
# ==========================================================
def recv_exact(sock, length):
    """Receive exactly 'length' bytes"""
    data = b""
    while len(data) < length:
        try:
            chunk = sock.recv(length - len(data))
            if not chunk:
                return None
            data += chunk
        except socket.timeout:
            return None
    return data

def build_someip_header(service_id, method_id, msg_type, payload_len=0):
    """Build SOME/IP header"""
    global session_id
    
    length = 8 + payload_len
    
    hdr = struct.pack(
        HEADER_FMT,
        service_id,
        method_id,
        length,
        CLIENT_ID,
        session_id,
        SOMEIP_PROTOCOL_VERSION,
        SOMEIP_INTERFACE_VERSION,
        msg_type,
        0x00  # return code
    )
    
    session_id = (session_id + 1) & 0xFFFF
    return hdr

def build_request(service_id, method_id, payload=b""):
    """Build REQUEST message"""
    hdr = build_someip_header(service_id, method_id, SOMEIP_MSG_REQUEST, len(payload))
    
    log(f"TX REQUEST: SID=0x{service_id:04X} MID=0x{method_id:04X} LEN={len(payload)}")
    log(f"TX HEADER: {hexdump(hdr)}")
    if payload:
        log(f"TX PAYLOAD: {hexdump(payload)}")
    
    return hdr + payload

def build_subscribe(service_id, eventgroup_id, ttl_seconds=5):
    """Build SUBSCRIBE message with event group and TTL"""
    # Payload: [eventgroup_id: 2 bytes] [ttl: 4 bytes]
    payload = struct.pack("!HI", eventgroup_id, ttl_seconds)
    
    hdr = build_someip_header(service_id, METHOD_SUBSCRIBE, SOMEIP_MSG_REQUEST, len(payload))
    
    log(f"TX SUBSCRIBE: SID=0x{service_id:04X} EG=0x{eventgroup_id:04X} TTL={ttl_seconds}s")
    log(f"TX HEADER: {hexdump(hdr)}")
    log(f"TX PAYLOAD: {hexdump(payload)}")
    
    return hdr + payload

def build_unsubscribe(service_id, eventgroup_id):
    """Build UNSUBSCRIBE message"""
    payload = struct.pack("!H", eventgroup_id)
    
    hdr = build_someip_header(service_id, METHOD_UNSUBSCRIBE, SOMEIP_MSG_REQUEST, len(payload))
    
    log(f"TX UNSUBSCRIBE: SID=0x{service_id:04X} EG=0x{eventgroup_id:04X}")
    log(f"TX HEADER: {hexdump(hdr)}")
    log(f"TX PAYLOAD: {hexdump(payload)}")
    
    return hdr + payload

# ==========================================================
# Service Discovery
# ==========================================================
def sd_find_services():
    """Query available services via SD"""
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
# Receiver Thread
# ==========================================================
def receiver(sock):
    """Receive and process SOME/IP messages"""
    global running

    log("RX THREAD: Started")

    while running:
        hdr_raw = recv_exact(sock, HEADER_SIZE)
        if hdr_raw is None:
            if running:  # Only log if unexpected
                log("RX THREAD: Connection closed by server")
            running = False
            break

        log(f"RX HEADER: {hexdump(hdr_raw)}")

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

        log(f"RX: SID=0x{service_id:04X} MID=0x{method_id:04X} "
            f"TYPE=0x{msg_type:02X} LEN={length}")

        if length < 8:
            log("RX ERROR: Invalid length")
            continue

        payload_len = length - 8
        payload = recv_exact(sock, payload_len) if payload_len > 0 else b""

        if payload_len > 0 and payload:
            log(f"RX PAYLOAD: {hexdump(payload)}")

        # Handle NOTIFICATION
        if msg_type == SOMEIP_MSG_NOTIFICATION:
            if service_id == SERVICE_HEARTBEAT and len(payload) == 4:
                alive = struct.unpack("!I", payload)[0]
                log(f"RX NOTIFICATION: Heartbeat alive = {alive}")
            else:
                log("RX NOTIFICATION: Unknown")
            continue

        # Handle ERROR
        if msg_type == SOMEIP_MSG_ERROR or ret != 0:
            log(f"RX ERROR: SID=0x{service_id:04X} MID=0x{method_id:04X}")
            continue

        # Handle RESPONSE
        log("RX RESPONSE: ACK received")

    log("RX THREAD: Exiting")

# ==========================================================
# Main Test Scenario
# ==========================================================
def main():
    global running
    
    log("CLIENT: Phase 4 Enhanced Client Starting")
    time.sleep(2)

    # Phase 1: Service Discovery
    sd_find_services()
    time.sleep(1)

    # Phase 2: Establish persistent TCP connection
    log(f"CLIENT: Connecting to {SERVER_IP}:{SERVER_PORT}")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)  # 5 second timeout for receives
    
    try:
        sock.connect((SERVER_IP, SERVER_PORT))
        log("CLIENT: TCP connected (persistent)")
    except Exception as e:
        log(f"CLIENT: Connection failed: {e}")
        return

    # Start receiver thread
    rx_thread = threading.Thread(
        target=receiver,
        args=(sock,),
        daemon=True
    )
    rx_thread.start()

    # Phase 3: Subscribe to heartbeat event group
    log("CLIENT: Subscribing to heartbeat event group")
    sock.sendall(build_subscribe(SERVICE_HEARTBEAT, EVENTGROUP_STATUS, ttl_seconds=10))
    time.sleep(0.5)

    # Phase 4: Periodic heartbeat requests (while receiving notifications)
    try:
        request_count = 0
        while running and request_count < 3:
            time.sleep(5)
            if not running:
                break
            
            log("CLIENT: Sending heartbeat REQUEST")
            sock.sendall(build_request(SERVICE_HEARTBEAT, METHOD_HEARTBEAT))
            request_count += 1

    except KeyboardInterrupt:
        log("CLIENT: KeyboardInterrupt received")

    finally:
        # Phase 5: Clean unsubscribe
        log("CLIENT: Sending UNSUBSCRIBE")
        try:
            sock.sendall(build_unsubscribe(SERVICE_HEARTBEAT, EVENTGROUP_STATUS))
            time.sleep(1.0)  # Wait for ACK
        except Exception as e:
            log(f"CLIENT: Unsubscribe error: {e}")

        running = False
        sock.close()
        log("CLIENT: Connection closed")
        log("CLIENT: Exit")

if __name__ == "__main__":
    main()