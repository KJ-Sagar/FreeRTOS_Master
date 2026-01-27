#!/usr/bin/env python3
"""
SOME/IP Production Client with Extensive Logging
=================================================
This client provides comprehensive logging for debugging and test validation.
Each message and state change is logged with detailed information.

Features:
- Color-coded log levels
- Message field breakdown
- State machine tracking
- Performance metrics
- Error detection and reporting
"""

import socket
import struct
import threading
import time
import sys
from datetime import datetime
from enum import Enum
from collections import defaultdict
from typing import Optional, Dict, List, Tuple

# ============================================================================
# Configuration
# ============================================================================
SERVER_IP = "10.0.0.2"
SERVER_PORT = 30509
CLIENT_IP = "10.0.0.1"
SD_UDP_PORT = 30490

# ============================================================================
# SOME/IP Protocol Constants
# ============================================================================
class MessageType(Enum):
    REQUEST = 0x00
    REQUEST_NO_RETURN = 0x01
    NOTIFICATION = 0x02
    RESPONSE = 0x80
    ERROR = 0x81

class ReturnCode(Enum):
    E_OK = 0x00
    E_NOT_OK = 0x01
    E_UNKNOWN_SERVICE = 0x02
    E_UNKNOWN_METHOD = 0x03
    E_NOT_READY = 0x04
    E_NOT_REACHABLE = 0x05
    E_TIMEOUT = 0x06
    E_WRONG_PROTOCOL_VERSION = 0x07
    E_WRONG_INTERFACE_VERSION = 0x08
    E_MALFORMED_MESSAGE = 0x09
    E_WRONG_MESSAGE_TYPE = 0x0A

# Protocol versions
SOMEIP_PROTOCOL_VERSION = 0x01
SOMEIP_INTERFACE_VERSION = 0x01

# Service IDs (from server code)
SERVICE_HEARTBEAT = 0x1234
SERVICE_SENSOR = 0x5678
SERVICE_ENGINE = 0x9ABC

# Method IDs
METHOD_HEARTBEAT = 0x0001
METHOD_SUBSCRIBE = 0x0100
METHOD_UNSUBSCRIBE = 0x0101
METHOD_GET_STATUS = 0x0002

# Event Groups
EVENTGROUP_STATUS = 0x0001
EVENTGROUP_SENSOR = 0x0002
EVENTGROUP_ENGINE = 0x0003

# Message structure
HEADER_FMT = "!HHIHHBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FMT)

# ============================================================================
# Color Codes for Terminal Output
# ============================================================================
class Color:
    """ANSI color codes for terminal output"""
    RESET = '\033[0m'
    BOLD = '\033[1m'
    
    # Foreground colors
    BLACK = '\033[30m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'
    
    # Background colors
    BG_RED = '\033[41m'
    BG_GREEN = '\033[42m'
    BG_YELLOW = '\033[43m'
    BG_BLUE = '\033[44m'

# ============================================================================
# Logging System
# ============================================================================
class LogLevel(Enum):
    """Log severity levels"""
    TRACE = 0    # Most detailed - every byte
    DEBUG = 1    # Debug information
    INFO = 2     # General information
    WARN = 3     # Warnings
    ERROR = 4    # Errors
    CRITICAL = 5 # Critical errors

class Logger:
    """Enhanced logging with color coding and formatting"""
    
    def __init__(self, level: LogLevel = LogLevel.DEBUG):
        self.level = level
        self.message_count = defaultdict(int)
        self.start_time = time.time()
        
    def _timestamp(self) -> str:
        """Get formatted timestamp"""
        return datetime.now().strftime("%H:%M:%S.%f")[:-3]
    
    def _elapsed(self) -> str:
        """Get elapsed time since start"""
        elapsed = time.time() - self.start_time
        return f"{elapsed:7.3f}s"
    
    def _format_message(self, level: LogLevel, msg: str, color: str = "") -> str:
        """Format log message with timestamp and level"""
        level_str = level.name.ljust(8)
        ts = self._timestamp()
        elapsed = self._elapsed()
        return f"{color}[{ts}] [{elapsed}] {level_str} | {msg}{Color.RESET}"
    
    def trace(self, msg: str):
        """Trace level logging (most detailed)"""
        if self.level.value <= LogLevel.TRACE.value:
            print(self._format_message(LogLevel.TRACE, msg, Color.WHITE), flush=True)
    
    def debug(self, msg: str):
        """Debug level logging"""
        if self.level.value <= LogLevel.DEBUG.value:
            print(self._format_message(LogLevel.DEBUG, msg, Color.CYAN), flush=True)
    
    def info(self, msg: str):
        """Info level logging"""
        if self.level.value <= LogLevel.INFO.value:
            print(self._format_message(LogLevel.INFO, msg, Color.GREEN), flush=True)
    
    def warn(self, msg: str):
        """Warning level logging"""
        if self.level.value <= LogLevel.WARN.value:
            print(self._format_message(LogLevel.WARN, msg, Color.YELLOW), flush=True)
    
    def error(self, msg: str):
        """Error level logging"""
        if self.level.value <= LogLevel.ERROR.value:
            print(self._format_message(LogLevel.ERROR, msg, Color.RED), flush=True)
    
    def critical(self, msg: str):
        """Critical error logging"""
        if self.level.value <= LogLevel.CRITICAL.value:
            print(self._format_message(LogLevel.CRITICAL, msg, 
                                     f"{Color.BG_RED}{Color.WHITE}{Color.BOLD}"), flush=True)
    
    def separator(self, title: str = "", char: str = "="):
        """Print a separator line"""
        width = 100
        if title:
            title_str = f" {title} "
            padding = (width - len(title_str)) // 2
            line = char * padding + title_str + char * padding
            print(f"\n{Color.BOLD}{Color.BLUE}{line}{Color.RESET}\n", flush=True)
        else:
            print(f"{Color.BLUE}{char * width}{Color.RESET}", flush=True)
    
    def header(self, msg: str):
        """Print a header message"""
        print(f"\n{Color.BOLD}{Color.MAGENTA}>>> {msg}{Color.RESET}\n", flush=True)
    
    def message_stats(self) -> Dict[str, int]:
        """Get message statistics"""
        return dict(self.message_count)

# Global logger instance
logger = Logger(LogLevel.DEBUG)

# ============================================================================
# SOME/IP Message Structures
# ============================================================================
class SomeIPHeader:
    """SOME/IP message header"""
    
    def __init__(self):
        self.service_id: int = 0
        self.method_id: int = 0
        self.length: int = 0
        self.client_id: int = 0
        self.session_id: int = 0
        self.protocol_version: int = SOMEIP_PROTOCOL_VERSION
        self.interface_version: int = SOMEIP_INTERFACE_VERSION
        self.message_type: int = MessageType.REQUEST.value
        self.return_code: int = ReturnCode.E_OK.value
    
    def pack(self) -> bytes:
        """Pack header to bytes (network byte order)"""
        return struct.pack(
            HEADER_FMT,
            self.service_id,
            self.method_id,
            self.length,
            self.client_id,
            self.session_id,
            self.protocol_version,
            self.interface_version,
            self.message_type,
            self.return_code
        )
    
    @staticmethod
    def unpack(data: bytes) -> 'SomeIPHeader':
        """Unpack header from bytes"""
        if len(data) < HEADER_SIZE:
            raise ValueError(f"Insufficient data for header: {len(data)} < {HEADER_SIZE}")
        
        values = struct.unpack(HEADER_FMT, data[:HEADER_SIZE])
        
        hdr = SomeIPHeader()
        hdr.service_id = values[0]
        hdr.method_id = values[1]
        hdr.length = values[2]
        hdr.client_id = values[3]
        hdr.session_id = values[4]
        hdr.protocol_version = values[5]
        hdr.interface_version = values[6]
        hdr.message_type = values[7]
        hdr.return_code = values[8]
        
        return hdr
    
    def __str__(self) -> str:
        """String representation for logging"""
        msg_type_name = MessageType(self.message_type).name if self.message_type in [m.value for m in MessageType] else f"UNKNOWN(0x{self.message_type:02X})"
        ret_code_name = ReturnCode(self.return_code).name if self.return_code in [r.value for r in ReturnCode] else f"UNKNOWN(0x{self.return_code:02X})"
        
        return (f"SomeIPHeader("
                f"SID=0x{self.service_id:04X}, "
                f"MID=0x{self.method_id:04X}, "
                f"Len={self.length}, "
                f"CID=0x{self.client_id:04X}, "
                f"SessID=0x{self.session_id:04X}, "
                f"Proto=0x{self.protocol_version:02X}, "
                f"Iface=0x{self.interface_version:02X}, "
                f"Type={msg_type_name}, "
                f"Ret={ret_code_name})")

# ============================================================================
# SOME/IP Client State Machine
# ============================================================================
class ClientState(Enum):
    """Client connection states"""
    DISCONNECTED = 0
    CONNECTING = 1
    CONNECTED = 2
    SUBSCRIBING = 3
    ACTIVE = 4
    UNSUBSCRIBING = 5
    CLOSING = 6
    ERROR = 7

class SomeIPClient:
    """
    SOME/IP TCP Client with extensive logging
    
    This client provides:
    - Connection management
    - Message sending/receiving
    - Subscription handling
    - State tracking
    - Performance metrics
    """
    
    def __init__(self, server_ip: str, server_port: int, client_id: int = 0x0001):
        self.server_ip = server_ip
        self.server_port = server_port
        self.client_id = client_id
        
        # Socket
        self.socket: Optional[socket.socket] = None
        
        # State
        self.state = ClientState.DISCONNECTED
        self.running = False
        
        # Session management
        self.session_id = 1
        
        # Subscriptions
        self.subscriptions: Dict[Tuple[int, int], Dict] = {}  # (service_id, eventgroup_id) -> sub_info
        
        # Statistics
        self.messages_sent = 0
        self.messages_received = 0
        self.notifications_received = 0
        self.errors = 0
        self.last_rx_time = 0
        self.last_tx_time = 0
        
        # Receive thread
        self.rx_thread: Optional[threading.Thread] = None
        
        logger.debug(f"SomeIPClient created: {server_ip}:{server_port}, ClientID=0x{client_id:04X}")
    
    def _next_session_id(self) -> int:
        """Get next session ID"""
        sid = self.session_id
        self.session_id = (self.session_id + 1) & 0xFFFF
        return sid
    
    def connect(self, timeout: float = 5.0) -> bool:
        """
        Connect to SOME/IP server
        
        Args:
            timeout: Connection timeout in seconds
            
        Returns:
            True if connected successfully
        """
        logger.header(f"CONNECT: Connecting to {self.server_ip}:{self.server_port}")
        
        self._change_state(ClientState.CONNECTING)
        
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(timeout)
            
            logger.debug(f"TCP socket created, attempting connection...")
            
            start_time = time.time()
            self.socket.connect((self.server_ip, self.server_port))
            connect_time = time.time() - start_time
            
            logger.info(f"✓ Connected successfully in {connect_time*1000:.1f}ms")
            
            # Get local address
            local_addr = self.socket.getsockname()
            logger.debug(f"Local address: {local_addr[0]}:{local_addr[1]}")
            
            self._change_state(ClientState.CONNECTED)
            
            # Start receive thread
            self.running = True
            self.rx_thread = threading.Thread(target=self._receive_loop, daemon=True)
            self.rx_thread.start()
            logger.debug("Receive thread started")
            
            return True
            
        except socket.timeout:
            logger.error(f"✗ Connection timeout after {timeout}s")
            self._change_state(ClientState.ERROR)
            return False
        except Exception as e:
            logger.error(f"✗ Connection failed: {e}")
            self._change_state(ClientState.ERROR)
            return False
    
    def _change_state(self, new_state: ClientState):
        """Change client state with logging"""
        if self.state != new_state:
            logger.info(f"STATE CHANGE: {self.state.name} -> {new_state.name}")
            self.state = new_state
    
    def _recv_exact(self, length: int) -> Optional[bytes]:
        """Receive exact number of bytes"""
        data = b""
        while len(data) < length:
            try:
                chunk = self.socket.recv(length - len(data))
                if not chunk:
                    logger.warn("Connection closed by server")
                    return None
                data += chunk
            except socket.timeout:
                logger.trace(f"Receive timeout (got {len(data)}/{length} bytes)")
                return None
            except Exception as e:
                logger.error(f"Receive error: {e}")
                return None
        return data
    
    def _receive_loop(self):
        """Main receive loop (runs in separate thread)"""
        logger.debug("RX THREAD: Started")
        
        while self.running:
            try:
                # Receive header
                hdr_data = self._recv_exact(HEADER_SIZE)
                if hdr_data is None:
                    if self.running:
                        logger.warn("RX THREAD: Connection lost")
                    break
                
                # Parse header
                hdr = SomeIPHeader.unpack(hdr_data)
                self.last_rx_time = time.time()
                self.messages_received += 1
                
                logger.debug(f"← RX HEADER: {hdr}")
                
                # Receive payload
                payload_len = hdr.length - 8 if hdr.length > 8 else 0
                payload = b""
                
                if payload_len > 0:
                    logger.trace(f"Receiving payload: {payload_len} bytes")
                    payload = self._recv_exact(payload_len)
                    if payload is None:
                        logger.error("Failed to receive payload")
                        break
                
                # Process message
                self._handle_message(hdr, payload)
                
            except Exception as e:
                logger.error(f"RX THREAD: Exception: {e}")
                if self.running:
                    self.errors += 1
                break
        
        logger.debug("RX THREAD: Exited")
        self.running = False
    
    def _handle_message(self, hdr: SomeIPHeader, payload: bytes):
        """Handle received message based on type"""
        
        logger.message_count[MessageType(hdr.message_type).name] += 1
        
        if hdr.message_type == MessageType.NOTIFICATION.value:
            self._handle_notification(hdr, payload)
        elif hdr.message_type == MessageType.RESPONSE.value:
            self._handle_response(hdr, payload)
        elif hdr.message_type == MessageType.ERROR.value:
            self._handle_error(hdr, payload)
        else:
            logger.warn(f"Unexpected message type: 0x{hdr.message_type:02X}")
    
    def _handle_notification(self, hdr: SomeIPHeader, payload: bytes):
        """Handle notification message"""
        self.notifications_received += 1
        
        logger.info(f"← NOTIFICATION: SID=0x{hdr.service_id:04X} MID=0x{hdr.method_id:04X} PayloadLen={len(payload)}")
        
        # Parse heartbeat notification
        if hdr.service_id == SERVICE_HEARTBEAT and len(payload) == 4:
            counter = struct.unpack("!I", payload)[0]
            logger.info(f"   └─ Heartbeat Counter: {counter}")
        else:
            logger.debug(f"   └─ Payload: {payload.hex()}")
    
    def _handle_response(self, hdr: SomeIPHeader, payload: bytes):
        """Handle response message"""
        ret_code = ReturnCode(hdr.return_code) if hdr.return_code in [r.value for r in ReturnCode] else None
        
        if hdr.return_code == ReturnCode.E_OK.value:
            logger.info(f"← RESPONSE: SID=0x{hdr.service_id:04X} MID=0x{hdr.method_id:04X} Status=OK")
        else:
            ret_name = ret_code.name if ret_code else f"0x{hdr.return_code:02X}"
            logger.error(f"← RESPONSE: SID=0x{hdr.service_id:04X} MID=0x{hdr.method_id:04X} Status={ret_name}")
            self.errors += 1
        
        # Log protocol/interface version verification
        if hdr.protocol_version != SOMEIP_PROTOCOL_VERSION:
            logger.warn(f"   └─ Protocol version mismatch: got 0x{hdr.protocol_version:02X}, expected 0x{SOMEIP_PROTOCOL_VERSION:02X}")
        
        if hdr.interface_version != SOMEIP_INTERFACE_VERSION:
            logger.warn(f"   └─ Interface version mismatch: got 0x{hdr.interface_version:02X}, expected 0x{SOMEIP_INTERFACE_VERSION:02X}")
    
    def _handle_error(self, hdr: SomeIPHeader, payload: bytes):
        """Handle error message"""
        ret_code = ReturnCode(hdr.return_code) if hdr.return_code in [r.value for r in ReturnCode] else None
        ret_name = ret_code.name if ret_code else f"0x{hdr.return_code:02X}"
        
        logger.error(f"← ERROR: SID=0x{hdr.service_id:04X} MID=0x{hdr.method_id:04X} Code={ret_name}")
        self.errors += 1
    
    def _send_message(self, hdr: SomeIPHeader, payload: bytes = b"") -> bool:
        """Send SOME/IP message"""
        if not self.socket:
            logger.error("Cannot send: not connected")
            return False
        
        # Update header fields
        hdr.client_id = self.client_id
        hdr.session_id = self._next_session_id()
        hdr.length = 8 + len(payload)
        
        # Pack message
        message = hdr.pack() + payload
        
        try:
            logger.debug(f"→ TX: {hdr}")
            if len(payload) > 0:
                logger.trace(f"   └─ Payload ({len(payload)} bytes): {payload.hex()}")
            
            self.socket.sendall(message)
            self.messages_sent += 1
            self.last_tx_time = time.time()
            
            return True
            
        except Exception as e:
            logger.error(f"Send failed: {e}")
            self.errors += 1
            return False
    
    def request(self, service_id: int, method_id: int, payload: bytes = b"") -> bool:
        """
        Send a SOME/IP request
        
        Args:
            service_id: Service ID
            method_id: Method ID
            payload: Request payload
            
        Returns:
            True if sent successfully
        """
        logger.header(f"REQUEST: SID=0x{service_id:04X} MID=0x{method_id:04X}")
        
        hdr = SomeIPHeader()
        hdr.service_id = service_id
        hdr.method_id = method_id
        hdr.message_type = MessageType.REQUEST.value
        
        return self._send_message(hdr, payload)
    
    def subscribe(self, service_id: int, eventgroup_id: int, ttl: int = 15) -> bool:
        """
        Subscribe to event group
        
        Args:
            service_id: Service ID
            eventgroup_id: Event group ID
            ttl: Time-to-live in seconds
            
        Returns:
            True if subscription sent successfully
        """
        logger.header(f"SUBSCRIBE: SID=0x{service_id:04X} EG=0x{eventgroup_id:04X} TTL={ttl}s")
        
        self._change_state(ClientState.SUBSCRIBING)
        
        # Build subscription payload (eventgroup_id + ttl)
        payload = struct.pack("!HI", eventgroup_id, ttl)
        
        hdr = SomeIPHeader()
        hdr.service_id = service_id
        hdr.method_id = METHOD_SUBSCRIBE
        hdr.message_type = MessageType.REQUEST.value
        
        result = self._send_message(hdr, payload)
        
        if result:
            # Track subscription
            sub_key = (service_id, eventgroup_id)
            self.subscriptions[sub_key] = {
                'ttl': ttl,
                'subscribed_at': time.time(),
                'expires_at': time.time() + ttl
            }
            logger.info(f"Subscription tracked: will expire in {ttl}s")
            self._change_state(ClientState.ACTIVE)
        
        return result
    
    def unsubscribe(self, service_id: int, eventgroup_id: int) -> bool:
        """
        Unsubscribe from event group
        
        Args:
            service_id: Service ID
            eventgroup_id: Event group ID
            
        Returns:
            True if unsubscription sent successfully
        """
        logger.header(f"UNSUBSCRIBE: SID=0x{service_id:04X} EG=0x{eventgroup_id:04X}")
        
        self._change_state(ClientState.UNSUBSCRIBING)
        
        # Build unsubscription payload (just eventgroup_id)
        payload = struct.pack("!H", eventgroup_id)
        
        hdr = SomeIPHeader()
        hdr.service_id = service_id
        hdr.method_id = METHOD_UNSUBSCRIBE
        hdr.message_type = MessageType.REQUEST.value
        
        result = self._send_message(hdr, payload)
        
        if result:
            # Remove subscription
            sub_key = (service_id, eventgroup_id)
            if sub_key in self.subscriptions:
                del self.subscriptions[sub_key]
                logger.info(f"Subscription removed from tracking")
            
            self._change_state(ClientState.CONNECTED)
        
        return result
    
    def check_subscription_expiry(self):
        """Check if any subscriptions have expired"""
        current_time = time.time()
        expired = []
        
        for sub_key, sub_info in self.subscriptions.items():
            if current_time > sub_info['expires_at']:
                expired.append(sub_key)
        
        if expired:
            logger.warn(f"Subscriptions expired: {len(expired)}")
            for sub_key in expired:
                service_id, eventgroup_id = sub_key
                logger.warn(f"   └─ SID=0x{service_id:04X} EG=0x{eventgroup_id:04X}")
                del self.subscriptions[sub_key]
    
    def disconnect(self):
        """Disconnect from server"""
        logger.header("DISCONNECT: Closing connection")
        
        self._change_state(ClientState.CLOSING)
        self.running = False
        
        if self.socket:
            try:
                self.socket.close()
                logger.info("✓ Socket closed")
            except Exception as e:
                logger.error(f"Error closing socket: {e}")
        
        if self.rx_thread and self.rx_thread.is_alive():
            logger.debug("Waiting for RX thread to exit...")
            self.rx_thread.join(timeout=2.0)
        
        self._change_state(ClientState.DISCONNECTED)
    
    def print_statistics(self):
        """Print connection statistics"""
        logger.separator("CLIENT STATISTICS")
        
        print(f"Connection:")
        print(f"  Server: {self.server_ip}:{self.server_port}")
        print(f"  Client ID: 0x{self.client_id:04X}")
        print(f"  State: {self.state.name}")
        print()
        
        print(f"Messages:")
        print(f"  Sent: {self.messages_sent}")
        print(f"  Received: {self.messages_received}")
        print(f"  Notifications: {self.notifications_received}")
        print(f"  Errors: {self.errors}")
        print()
        
        print(f"Message Types:")
        for msg_type, count in logger.message_stats().items():
            print(f"  {msg_type:20s}: {count:4d}")
        print()
        
        print(f"Subscriptions:")
        if self.subscriptions:
            current_time = time.time()
            for sub_key, sub_info in self.subscriptions.items():
                service_id, eventgroup_id = sub_key
                time_left = sub_info['expires_at'] - current_time
                print(f"  SID=0x{service_id:04X} EG=0x{eventgroup_id:04X}: "
                      f"TTL={sub_info['ttl']}s, Expires in {time_left:.1f}s")
        else:
            print(f"  None")
        print()
        
        logger.separator()

# ============================================================================
# Main (Example Usage)
# ============================================================================
def main():
    """Example usage of SOME/IP client"""
    
    logger.separator("SOME/IP CLIENT - PRODUCTION VERSION WITH EXTENSIVE LOGGING")
    logger.info(f"Server: {SERVER_IP}:{SERVER_PORT}")
    logger.info(f"Client IP: {CLIENT_IP}")
    logger.info(f"Start Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    logger.separator()
    
    # Create client
    client = SomeIPClient(SERVER_IP, SERVER_PORT)
    
    try:
        # Connect
        if not client.connect():
            logger.critical("Failed to connect to server")
            return 1
        
        time.sleep(0.5)
        
        # Subscribe to heartbeat
        if not client.subscribe(SERVICE_HEARTBEAT, EVENTGROUP_STATUS, ttl=15):
            logger.error("Failed to subscribe")
            return 1
        
        time.sleep(1)
        
        # Send some requests
        for i in range(3):
            logger.header(f"Sending request #{i+1}")
            client.request(SERVICE_HEARTBEAT, METHOD_HEARTBEAT)
            time.sleep(5)
            
            if not client.running:
                break
        
        # Check subscription expiry
        client.check_subscription_expiry()
        
        # Unsubscribe
        client.unsubscribe(SERVICE_HEARTBEAT, EVENTGROUP_STATUS)
        
        time.sleep(1)
        
        # Print statistics
        client.print_statistics()
        
    except KeyboardInterrupt:
        logger.warn("\nInterrupted by user")
    except Exception as e:
        logger.critical(f"Unexpected error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        # Disconnect
        client.disconnect()
        logger.info("Test complete")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
