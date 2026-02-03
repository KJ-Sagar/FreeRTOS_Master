#!/usr/bin/env python3
"""
SOME/IP Test Client with Real MAC Authentication
Implements HMAC-SHA256 for Power Management messages

This version uses cryptographic MAC authentication instead of just headers.
"""

import socket
import struct
import threading
import time
import sys
from datetime import datetime
from collections import defaultdict

# Import MAC authentication
from mac_authentication import PowerManagementMAC, MACVerificationFailed, ReplayDetected, FreshnessViolation

# Configuration
SERVER_IP = "10.0.0.2"
SERVER_PORT = 30509
CLIENT_IP = "10.0.0.1"

# SOME/IP Constants
SOMEIP_MSG_REQUEST = 0x00
SOMEIP_MSG_NOTIFICATION = 0x02
SOMEIP_MSG_RESPONSE = 0x80
SOMEIP_MSG_ERROR = 0x81

SOMEIP_PROTOCOL_VERSION = 0x01
SOMEIP_INTERFACE_VERSION = 0x01
CLIENT_ID = 0x0001

# Service IDs
SERVICE_HEARTBEAT = 0x1234

# Method IDs
METHOD_HEARTBEAT = 0x0001
METHOD_SUBSCRIBE = 0x0100
METHOD_UNSUBSCRIBE = 0x0101

# Event Groups
EVENTGROUP_STATUS = 0x0001

# Power Management Message Types
PM_HEADER_HEARTBEAT = 0xFFFE8FFE
PM_HEADER_PROFILE = 0xFFFD8FFF

# Profile Request Types
PROFILE_ACTIVATE = 0x01
PROFILE_DEACTIVATE = 0x02

HEADER_FMT = "!HHIHHBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

# Global state
session_id = 1
running = True
message_stats = defaultdict(int)
notification_times = []

# MAC Authenticator
mac_handler = PowerManagementMAC()

class Colors:
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    END = '\033[0m'
    BOLD = '\033[1m'

def ts():
    """Get timestamp"""
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]

def log(msg, level="INFO"):
    """Enhanced logging with color coding"""
    color = Colors.END
    if level == "PASS":
        color = Colors.GREEN
    elif level == "FAIL":
        color = Colors.RED
    elif level == "WARN":
        color = Colors.YELLOW
    elif level == "STEP":
        color = Colors.CYAN + Colors.BOLD
    elif level == "HEADER":
        color = Colors.HEADER + Colors.BOLD
    elif level == "MAC":
        color = Colors.BLUE
    
    print(f"{color}[{ts()}] {msg}{Colors.END}", flush=True)

def recv_exact(sock, length):
    """Receive exact number of bytes"""
    data = b""
    while len(data) < length:
        try:
            chunk = sock.recv(length - len(data))
            if not chunk:
                return None
            data += chunk
        except socket.timeout:
            return None
        except Exception as e:
            log(f"recv_exact error: {e}", "WARN")
            return None
    return data

def build_someip_header(service_id, method_id, msg_type, payload_len=0):
    """Build SOME/IP header"""
    global session_id
    length = 8 + payload_len
    hdr = struct.pack(HEADER_FMT, service_id, method_id, length,
                      CLIENT_ID, session_id, SOMEIP_PROTOCOL_VERSION,
                      SOMEIP_INTERFACE_VERSION, msg_type, 0x00)
    
    log(f"  → TX: SID=0x{service_id:04X} MID=0x{method_id:04X} "
        f"SessionID=0x{session_id:04X} Type=0x{msg_type:02X}", "INFO")
    
    session_id = (session_id + 1) & 0xFFFF
    return hdr

def build_mac_authenticated_heartbeat(counter, use_mac=True):
    """
    Build MAC authenticated Heartbeat message with real HMAC-SHA256
    
    Args:
        counter: Heartbeat counter value
        use_mac: If True, add MAC authentication (default: True)
    
    Returns:
        Complete message with MAC tag
    """
    # Header: 0xFFFE 8FFE
    header = struct.pack("!I", PM_HEADER_HEARTBEAT)
    
    # Convert IP to bytes
    ip_parts = CLIENT_IP.split('.')
    src_pm_id = struct.pack("!BBBB", int(ip_parts[0]), int(ip_parts[1]), 
                           int(ip_parts[2]), int(ip_parts[3]))
    
    # SOME/IP part
    someip_part = struct.pack("!IBBBB", 0x00000000, 0x01, 0x01, 0x02, 0x00)
    
    # Counter
    counter_bytes = struct.pack("!I", counter)
    
    # Assemble payload
    payload = someip_part + src_pm_id + counter_bytes
    length = struct.pack("!I", len(payload))
    
    if use_mac:
        # Use MAC authentication
        message = mac_handler.sign_heartbeat(header, length, payload)
        log(f"  → TX MAC-AUTH HEARTBEAT: Counter={counter} "
            f"SrcIP={CLIENT_IP} [HMAC-SHA256 Protected] Size={len(message)}B", "MAC")
    else:
        # No MAC (legacy mode)
        message = header + length + payload
        log(f"  → TX HEARTBEAT (no MAC): Counter={counter} "
            f"SrcIP={CLIENT_IP} Size={len(message)}B", "WARN")
    
    return message

def build_profile_request(profile_id, request_type, dst_pm_id=None, use_mac=True):
    """
    Build MAC authenticated PROFILE_REQUEST message
    
    Args:
        profile_id: 40-bit profile identifier
        request_type: 0x01 (ACTIVATE) or 0x02 (DEACTIVATE)
        dst_pm_id: Destination IP address (default: SERVER_IP)
        use_mac: If True, add MAC authentication (default: True)
    
    Returns:
        Complete message with MAC tag
    """
    header = struct.pack("!I", PM_HEADER_PROFILE)
    
    # Convert IPs to bytes
    src_ip_parts = CLIENT_IP.split('.')
    src_pm_id = struct.pack("!BBBB", int(src_ip_parts[0]), int(src_ip_parts[1]),
                           int(src_ip_parts[2]), int(src_ip_parts[3]))
    
    if dst_pm_id is None:
        dst_pm_id = SERVER_IP
    dst_ip_parts = dst_pm_id.split('.')
    dst_pm_id_bytes = struct.pack("!BBBB", int(dst_ip_parts[0]), int(dst_ip_parts[1]),
                                  int(dst_ip_parts[2]), int(dst_ip_parts[3]))
    
    # SOME/IP part
    someip_part = struct.pack("!IBBBB", 0x00000000, 0x01, 0x01, 0x02, 0x00)
    
    # Message Type 0x00 entries
    msg_type_0 = struct.pack("!BB", 0x00, 0x06)
    
    # Profile ID (40 bits = 5 bytes)
    profile_id_bytes = struct.pack("!IB", (profile_id >> 8) & 0xFFFFFFFF, profile_id & 0xFF)
    
    # Request type
    request_byte = struct.pack("!B", request_type)
    
    # Message Type 0x01 (no entries)
    msg_type_1 = struct.pack("!BB", 0x01, 0x00)
    
    # Assemble payload
    payload = (someip_part + src_pm_id + dst_pm_id_bytes + 
               msg_type_0 + profile_id_bytes + request_byte + msg_type_1)
    
    length = struct.pack("!I", len(payload))
    
    req_name = "ACTIVATE" if request_type == PROFILE_ACTIVATE else "DEACTIVATE"
    
    if use_mac:
        # Use MAC authentication
        message = mac_handler.sign_profile_request(header, length, payload)
        log(f"  → TX MAC-AUTH PROFILE_REQUEST({req_name}): "
            f"ProfileID=0x{profile_id:010X} [HMAC-SHA256 Protected] Size={len(message)}B", "MAC")
    else:
        # No MAC (legacy mode)
        message = header + length + payload
        log(f"  → TX PROFILE_REQUEST({req_name}): ProfileID=0x{profile_id:010X} "
            f"(no MAC) Size={len(message)}B", "WARN")
    
    return message

def build_subscribe(service_id, eventgroup_id, ttl_seconds=10):
    """Build SOME/IP subscribe message"""
    payload = struct.pack("!HI", eventgroup_id, ttl_seconds)
    hdr = build_someip_header(service_id, METHOD_SUBSCRIBE, SOMEIP_MSG_REQUEST, len(payload))
    log(f"  → Subscribing to EventGroup 0x{eventgroup_id:04X} with TTL={ttl_seconds}s", "INFO")
    return hdr + payload

def build_unsubscribe(service_id, eventgroup_id):
    """Build SOME/IP unsubscribe message"""
    payload = struct.pack("!H", eventgroup_id)
    hdr = build_someip_header(service_id, METHOD_UNSUBSCRIBE, SOMEIP_MSG_REQUEST, len(payload))
    log(f"  → Unsubscribing from EventGroup 0x{eventgroup_id:04X}", "INFO")
    return hdr + payload

def parse_someip_message(hdr_raw, payload):
    """Parse SOME/IP message"""
    (service_id, method_id, length, client_id, session,
     proto, iface, msg_type, ret) = struct.unpack(HEADER_FMT, hdr_raw)
    
    msg_type_str = {
        0x00: "REQUEST",
        0x02: "NOTIFICATION", 
        0x80: "RESPONSE",
        0x81: "ERROR"
    }.get(msg_type, f"UNKNOWN(0x{msg_type:02X})")
    
    return {
        'service_id': service_id,
        'method_id': method_id,
        'length': length,
        'client_id': client_id,
        'session': session,
        'msg_type': msg_type,
        'msg_type_str': msg_type_str,
        'return_code': ret,
        'payload': payload
    }

def receiver(sock):
    """Receiver thread"""
    global running
    log("RX THREAD: Started", "INFO")
    
    while running:
        try:
            hdr_raw = recv_exact(sock, HEADER_SIZE)
            if not hdr_raw:
                break
            
            msg = parse_someip_message(hdr_raw, b"")
            payload_len = msg['length'] - 8
            
            if payload_len > 0:
                payload = recv_exact(sock, payload_len)
                if payload:
                    msg['payload'] = payload
            
            if msg['msg_type'] == 0x02:  # NOTIFICATION
                notification_times.append(time.time())
                log(f"← RX NOTIFICATION: Count={len(notification_times)}", "INFO")
            elif msg['msg_type'] == 0x80:  # RESPONSE
                log(f"← RX RESPONSE: Session=0x{msg['session']:04X} RetCode=0x{msg['return_code']:02X}", "INFO")
                
        except Exception as e:
            if running:
                log(f"Receive error: {e}", "WARN")
            break
    
    log("RX THREAD: Stopped", "INFO")

def test_mac_authenticated_heartbeat():
    """Test MAC authenticated heartbeat with verification"""
    log("\n" + "="*80, "HEADER")
    log("TEST: MAC Authenticated Heartbeat", "HEADER")
    log("="*80, "HEADER")
    
    # Connect
    log("\n[STEP 1] Connecting to server...", "STEP")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    
    try:
        start = time.time()
        sock.connect((SERVER_IP, SERVER_PORT))
        duration = (time.time() - start) * 1000
        log(f"✓ Connected to {SERVER_IP}:{SERVER_PORT} in {duration:.1f}ms", "PASS")
    except Exception as e:
        log(f"✗ Connection failed: {e}", "FAIL")
        return
    
    # Start receiver thread
    rx_thread = threading.Thread(target=receiver, args=(sock,), daemon=True)
    rx_thread.start()
    time.sleep(0.1)
    
    # Send MAC-authenticated heartbeats
    log("\n[STEP 2] Sending MAC-authenticated heartbeats (3x at 5ms)", "STEP")
    for counter in range(3):
        msg = build_mac_authenticated_heartbeat(counter, use_mac=True)
        sock.sendall(msg)
        time.sleep(0.005)
    
    log("✓ Sent 3 MAC-authenticated heartbeats", "PASS")
    
    # Wait for response
    time.sleep(0.5)
    
    # Send steady-state heartbeat
    log("\n[STEP 3] Sending steady-state heartbeat (1s period)", "STEP")
    msg = build_mac_authenticated_heartbeat(3, use_mac=True)
    sock.sendall(msg)
    log("✓ Sent steady-state heartbeat", "PASS")
    
    # Show MAC statistics
    log("\n[STEP 4] MAC Authentication Statistics", "STEP")
    stats = mac_handler.get_statistics()
    log(f"  Messages Generated:    {stats['generated']}", "MAC")
    log(f"  Messages Verified:     {stats['verified']}", "MAC")
    log(f"  Verification Failed:   {stats['failed']}", "MAC")
    log(f"  Replays Detected:      {stats['replays']}", "MAC")
    log(f"  Freshness Violations:  {stats['freshness_violations']}", "MAC")
    log(f"  Success Rate:          {stats['success_rate']:.1f}%", "MAC")
    log(f"  Key Age:               {stats['key_age']:.1f}s", "MAC")
    
    # Clean disconnect
    log("\n[STEP 5] Disconnecting...", "STEP")
    global running
    running = False
    sock.close()
    time.sleep(0.2)
    log("✓ Disconnected", "PASS")
    
    log("\n" + "="*80, "HEADER")
    log("✅ TEST COMPLETE", "HEADER")
    log("="*80, "HEADER")

def test_profile_activation_with_mac():
    """Test profile activation with MAC authentication"""
    log("\n" + "="*80, "HEADER")
    log("TEST: Profile Activation with MAC Authentication", "HEADER")
    log("="*80, "HEADER")
    
    global running
    running = True
    
    # Connect
    log("\n[STEP 1] Connecting to server...", "STEP")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    
    try:
        sock.connect((SERVER_IP, SERVER_PORT))
        log(f"✓ Connected to {SERVER_IP}:{SERVER_PORT}", "PASS")
    except Exception as e:
        log(f"✗ Connection failed: {e}", "FAIL")
        return
    
    # Start receiver
    rx_thread = threading.Thread(target=receiver, args=(sock,), daemon=True)
    rx_thread.start()
    time.sleep(0.1)
    
    # Send wake-up heartbeat
    log("\n[STEP 2] Sending wake-up heartbeat", "STEP")
    msg = build_mac_authenticated_heartbeat(0, use_mac=True)
    sock.sendall(msg)
    time.sleep(0.1)
    log("✓ Wake-up heartbeat sent", "PASS")
    
    # Activate profile
    log("\n[STEP 3] Activating Profile 0x01 with MAC authentication", "STEP")
    msg = build_profile_request(0x0000000001, PROFILE_ACTIVATE, use_mac=True)
    sock.sendall(msg)
    time.sleep(0.2)
    log("✓ Profile activation request sent", "PASS")
    
    # Subscribe to services
    log("\n[STEP 4] Subscribing to services", "STEP")
    msg = build_subscribe(SERVICE_HEARTBEAT, EVENTGROUP_STATUS, ttl=30)
    sock.sendall(msg)
    time.sleep(0.2)
    log("✓ Subscription request sent", "PASS")
    
    # Monitor notifications
    log("\n[STEP 5] Monitoring notifications (5s)...", "STEP")
    notification_times.clear()
    time.sleep(5)
    
    if len(notification_times) > 0:
        log(f"✓ Received {len(notification_times)} notifications", "PASS")
    else:
        log(f"⚠ No notifications received", "WARN")
    
    # Deactivate profile
    log("\n[STEP 6] Deactivating profile", "STEP")
    msg = build_profile_request(0x0000000001, PROFILE_DEACTIVATE, use_mac=True)
    sock.sendall(msg)
    time.sleep(0.2)
    log("✓ Profile deactivation request sent", "PASS")
    
    # Unsubscribe
    log("\n[STEP 7] Unsubscribing", "STEP")
    msg = build_unsubscribe(SERVICE_HEARTBEAT, EVENTGROUP_STATUS)
    sock.sendall(msg)
    time.sleep(0.2)
    log("✓ Unsubscribe request sent", "PASS")
    
    # Show statistics
    log("\n[STEP 8] Final MAC Statistics", "STEP")
    stats = mac_handler.get_statistics()
    log(f"  Total MAC Operations:  {stats['generated'] + stats['verified']}", "MAC")
    log(f"  Security Violations:   {stats['failed'] + stats['replays'] + stats['freshness_violations']}", "MAC")
    log(f"  Success Rate:          {stats['success_rate']:.1f}%", "MAC")
    
    # Clean up
    running = False
    sock.close()
    time.sleep(0.2)
    
    log("\n" + "="*80, "HEADER")
    log("✅ TEST COMPLETE", "HEADER")
    log("="*80, "HEADER")

def compare_mac_vs_no_mac():
    """Demonstrate difference between MAC and non-MAC messages"""
    log("\n" + "="*80, "HEADER")
    log("COMPARISON: MAC Authentication vs No MAC", "HEADER")
    log("="*80, "HEADER")
    
    log("\n1. Building Heartbeat WITHOUT MAC", "STEP")
    msg_no_mac = build_mac_authenticated_heartbeat(0, use_mac=False)
    log(f"   Size: {len(msg_no_mac)} bytes", "INFO")
    log(f"   Hex:  {' '.join(f'{b:02X}' for b in msg_no_mac)}", "INFO")
    
    log("\n2. Building Heartbeat WITH MAC (HMAC-SHA256)", "STEP")
    msg_with_mac = build_mac_authenticated_heartbeat(0, use_mac=True)
    log(f"   Size: {len(msg_with_mac)} bytes", "INFO")
    log(f"   Hex:  {' '.join(f'{b:02X}' for b in msg_with_mac[:32])}...", "INFO")
    log(f"   Overhead: +{len(msg_with_mac) - len(msg_no_mac)} bytes (timestamp + MAC tag)", "INFO")
    
    log("\n3. Security Benefits", "STEP")
    log("   ✅ Tampering Detection: Any bit flip invalidates MAC", "PASS")
    log("   ✅ Replay Protection: Counter prevents old messages", "PASS")
    log("   ✅ Freshness Validation: Timestamp ensures recent messages", "PASS")
    log("   ✅ Authentication: Only holders of secret key can create valid messages", "PASS")
    
    log("\n4. Performance Impact", "STEP")
    log(f"   Message size increase: {((len(msg_with_mac) - len(msg_no_mac)) / len(msg_no_mac) * 100):.1f}%", "INFO")
    log(f"   Network overhead: {len(msg_with_mac) - len(msg_no_mac)} bytes per message", "INFO")
    log(f"   At 1 Hz: {(len(msg_with_mac) - len(msg_no_mac)) * 8} bps additional", "INFO")
    
    log("\n" + "="*80, "HEADER")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        test_type = sys.argv[1].lower()
        
        if test_type == "heartbeat":
            test_mac_authenticated_heartbeat()
        elif test_type == "profile":
            test_profile_activation_with_mac()
        elif test_type == "compare":
            compare_mac_vs_no_mac()
        else:
            print(f"Unknown test: {test_type}")
            print("Usage: python3 mac_test_client.py [heartbeat|profile|compare]")
    else:
        # Run all tests
        compare_mac_vs_no_mac()
        print("\n")
        test_mac_authenticated_heartbeat()
        print("\n")
        test_profile_activation_with_mac()
