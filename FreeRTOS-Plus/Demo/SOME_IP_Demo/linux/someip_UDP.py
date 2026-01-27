#!/usr/bin/env python3
"""
SOME/IP Python Client - Fully Persistent Connection
Zero reconnects, single TCP connection for entire session
"""
import socket
import struct
import threading
import time
from datetime import datetime

# Configuration
SERVER_IP = "10.0.0.2"
SERVER_PORT = 30509
SD_UDP_PORT = 30490

# SOME/IP Constants
SOMEIP_MSG_REQUEST = 0x00
SOMEIP_MSG_NOTIFICATION = 0x02
SOMEIP_MSG_RESPONSE = 0x80

SOMEIP_PROTOCOL_VERSION = 0x01
SOMEIP_INTERFACE_VERSION = 0x01
CLIENT_ID = 0x0001

SERVICE_HEARTBEAT = 0x1234
METHOD_HEARTBEAT = 0x0001
METHOD_SUBSCRIBE = 0x0100
METHOD_UNSUBSCRIBE = 0x0101
EVENTGROUP_STATUS = 0x0001

HEADER_FMT = "!HHIHHBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

session_id = 1
running = True

def ts():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]

def log(msg):
    print(f"[{ts()}] {msg}", flush=True)

def recv_exact(sock, length):
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
    global session_id
    length = 8 + payload_len
    hdr = struct.pack(HEADER_FMT, service_id, method_id, length,
                      CLIENT_ID, session_id, SOMEIP_PROTOCOL_VERSION,
                      SOMEIP_INTERFACE_VERSION, msg_type, 0x00)
    session_id = (session_id + 1) & 0xFFFF
    return hdr

def build_request(service_id, method_id, payload=b""):
    hdr = build_someip_header(service_id, method_id, SOMEIP_MSG_REQUEST, len(payload))
    log(f"TX REQUEST: SID=0x{service_id:04X} MID=0x{method_id:04X}")
    return hdr + payload

def build_subscribe(service_id, eventgroup_id, ttl_seconds=10):
    payload = struct.pack("!HI", eventgroup_id, ttl_seconds)
    hdr = build_someip_header(service_id, METHOD_SUBSCRIBE, SOMEIP_MSG_REQUEST, len(payload))
    log(f"TX SUBSCRIBE: SID=0x{service_id:04X} EG=0x{eventgroup_id:04X} TTL={ttl_seconds}s")
    return hdr + payload

def build_unsubscribe(service_id, eventgroup_id):
    payload = struct.pack("!H", eventgroup_id)
    hdr = build_someip_header(service_id, METHOD_UNSUBSCRIBE, SOMEIP_MSG_REQUEST, len(payload))
    log(f"TX UNSUBSCRIBE: SID=0x{service_id:04X} EG=0x{eventgroup_id:04X}")
    return hdr + payload

def receiver(sock):
    global running
    log("RX THREAD: Started")
    
    while running:
        hdr_raw = recv_exact(sock, HEADER_SIZE)
        if hdr_raw is None:
            if running:
                log("RX THREAD: Connection closed")
            running = False
            break
        
        (service_id, method_id, length, client_id, session,
         proto, iface, msg_type, ret) = struct.unpack(HEADER_FMT, hdr_raw)
        
        payload_len = length - 8
        payload = recv_exact(sock, payload_len) if payload_len > 0 else b""
        
        if msg_type == SOMEIP_MSG_NOTIFICATION:
            if service_id == SERVICE_HEARTBEAT and len(payload) == 4:
                alive = struct.unpack("!I", payload)[0]
                log(f"RX NOTIFICATION: Heartbeat={alive}")
        elif msg_type == SOMEIP_MSG_RESPONSE:
            log(f"RX RESPONSE: SID=0x{service_id:04X} MID=0x{method_id:04X}")
        elif msg_type == SOMEIP_MSG_REQUEST:
            log(f"RX ERROR: Unexpected REQUEST from server")
    
    log("RX THREAD: Exited")

def main():
    global running
    
    log("=== Phase 4 Persistent Client ===")
    time.sleep(1)
    
    # Connect
    log(f"Connecting to {SERVER_IP}:{SERVER_PORT}")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    
    try:
        sock.connect((SERVER_IP, SERVER_PORT))
        log("✓ Connected (single persistent connection)")
    except Exception as e:
        log(f"✗ Connection failed: {e}")
        return
    
    # Start receiver
    rx_thread = threading.Thread(target=receiver, args=(sock,), daemon=True)
    rx_thread.start()
    time.sleep(0.1)
    
    # Subscribe
    log("Subscribing to heartbeat eventgroup...")
    sock.sendall(build_subscribe(SERVICE_HEARTBEAT, EVENTGROUP_STATUS, ttl_seconds=15))
    time.sleep(0.5)
    
    # Send periodic requests on SAME connection
    try:
        for i in range(3):
            time.sleep(5)
            if not running:
                break
            log(f"Sending heartbeat request #{i+1} (same connection)")
            sock.sendall(build_request(SERVICE_HEARTBEAT, METHOD_HEARTBEAT))
    
    except KeyboardInterrupt:
        log("KeyboardInterrupt")
    except (BrokenPipeError, ConnectionResetError) as e:
        log(f"Connection error: {e}")
        running = False
    
    # Clean unsubscribe
    if running:
        log("Unsubscribing...")
        try:
            sock.sendall(build_unsubscribe(SERVICE_HEARTBEAT, EVENTGROUP_STATUS))
            time.sleep(1.0)
        except Exception as e:
            log(f"Unsubscribe error: {e}")
    
    running = False
    sock.close()
    log("✓ Connection closed")
    log("=== Test Complete ===")

if __name__ == "__main__":
    main()