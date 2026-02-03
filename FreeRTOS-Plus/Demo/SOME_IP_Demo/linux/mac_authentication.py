#!/usr/bin/env python3
"""
MAC Authentication Module for SOME/IP Power Management
Implements HMAC-SHA256 based Message Authentication Code

This module provides cryptographic authentication for Power Management messages,
preventing unauthorized ECU wake-up and ensuring message integrity.

Features:
- HMAC-SHA256 for message authentication
- Counter-based replay protection
- Key management and rotation
- Freshness validation
- Comprehensive logging
"""

import hmac
import hashlib
import struct
import time
from datetime import datetime
from collections import defaultdict
from typing import Optional, Tuple, Dict

# MAC Configuration
MAC_ALGORITHM = hashlib.sha256  # HMAC-SHA256
MAC_TAG_LENGTH = 32  # SHA256 produces 32-byte hash
MAC_TAG_TRUNCATED_LENGTH = 16  # Truncate to 16 bytes for efficiency

# Security Configuration
REPLAY_WINDOW = 100  # Accept messages within this counter window
FRESHNESS_TIMEOUT = 5.0  # Seconds - reject messages older than this
MAX_COUNTER_VALUE = 0xFFFFFFFF  # 32-bit counter max

# Key Management
DEFAULT_KEY = b"SecureKey_SOME_IP_Power_Management_2026"  # Development only!
KEY_ROTATION_INTERVAL = 3600  # Rotate keys every hour


class MACAuthenticationError(Exception):
    """Base exception for MAC authentication errors"""
    pass


class MACVerificationFailed(MACAuthenticationError):
    """Raised when MAC verification fails"""
    pass


class ReplayDetected(MACAuthenticationError):
    """Raised when replay attack detected"""
    pass


class CounterOverflow(MACAuthenticationError):
    """Raised when counter reaches maximum value"""
    pass


class FreshnessViolation(MACAuthenticationError):
    """Raised when message is too old"""
    pass


class MACAuthenticator:
    """
    Handles MAC authentication for Power Management messages
    
    Provides:
    - HMAC-SHA256 tag generation and verification
    - Replay attack protection
    - Freshness validation
    - Key management
    """
    
    def __init__(self, key: bytes = DEFAULT_KEY, truncate: bool = True):
        """
        Initialize MAC authenticator
        
        Args:
            key: Shared secret key for HMAC
            truncate: If True, truncate MAC to 16 bytes
        """
        self.key = key
        self.truncate = truncate
        self.mac_length = MAC_TAG_TRUNCATED_LENGTH if truncate else MAC_TAG_LENGTH
        
        # Replay protection: track last seen counter per source
        self.last_counters: Dict[str, int] = {}
        
        # Freshness tracking: store message timestamps
        self.message_timestamps: Dict[Tuple[str, int], float] = {}
        
        # Statistics
        self.stats = {
            'generated': 0,
            'verified': 0,
            'failed': 0,
            'replays': 0,
            'freshness_violations': 0
        }
        
        # Key rotation
        self.key_created_at = time.time()
        self.key_rotation_interval = KEY_ROTATION_INTERVAL
        
    def generate_mac(self, message: bytes, include_timestamp: bool = True) -> bytes:
        """
        Generate MAC tag for a message
        
        Args:
            message: The message to authenticate
            include_timestamp: If True, include current timestamp in MAC
            
        Returns:
            MAC tag (16 or 32 bytes depending on truncate setting)
        """
        # Optionally include timestamp for freshness
        if include_timestamp:
            timestamp = struct.pack("!d", time.time())  # 8 bytes, double
            data = message + timestamp
        else:
            data = message
        
        # Compute HMAC-SHA256
        mac = hmac.new(self.key, data, MAC_ALGORITHM)
        mac_tag = mac.digest()
        
        # Truncate if configured
        if self.truncate:
            mac_tag = mac_tag[:MAC_TAG_TRUNCATED_LENGTH]
        
        self.stats['generated'] += 1
        return mac_tag
    
    def verify_mac(self, message: bytes, mac_tag: bytes, 
                   check_freshness: bool = True) -> bool:
        """
        Verify MAC tag for a message
        
        Args:
            message: The message to verify
            mac_tag: The MAC tag to check
            check_freshness: If True, check message timestamp
            
        Returns:
            True if MAC is valid
            
        Raises:
            MACVerificationFailed: If MAC doesn't match
            FreshnessViolation: If message is too old
        """
        # Extract timestamp if present
        if check_freshness and len(message) >= 8:
            # Assume last 8 bytes are timestamp
            msg_data = message[:-8]
            timestamp_bytes = message[-8:]
            timestamp = struct.unpack("!d", timestamp_bytes)[0]
            
            # Check freshness
            age = time.time() - timestamp
            if age > FRESHNESS_TIMEOUT:
                self.stats['freshness_violations'] += 1
                raise FreshnessViolation(
                    f"Message too old: {age:.2f}s (max: {FRESHNESS_TIMEOUT}s)"
                )
        else:
            msg_data = message
        
        # Recompute MAC
        expected_mac = hmac.new(self.key, message, MAC_ALGORITHM).digest()
        if self.truncate:
            expected_mac = expected_mac[:MAC_TAG_TRUNCATED_LENGTH]
        
        # Constant-time comparison to prevent timing attacks
        if not hmac.compare_digest(mac_tag, expected_mac):
            self.stats['failed'] += 1
            raise MACVerificationFailed("MAC verification failed")
        
        self.stats['verified'] += 1
        return True
    
    def check_replay(self, source_id: str, counter: int) -> bool:
        """
        Check if message is a replay attack
        
        Args:
            source_id: Source identifier (e.g., IP address)
            counter: Message counter
            
        Returns:
            True if message is fresh (not a replay)
            
        Raises:
            ReplayDetected: If replay attack detected
        """
        last_counter = self.last_counters.get(source_id, -1)
        
        # Counter must be greater than last seen
        if counter <= last_counter:
            # Allow for out-of-order delivery within window
            if last_counter - counter > REPLAY_WINDOW:
                self.stats['replays'] += 1
                raise ReplayDetected(
                    f"Replay detected: counter {counter} <= last {last_counter}"
                )
        
        # Update last seen counter
        self.last_counters[source_id] = max(counter, last_counter)
        return True
    
    def rotate_key(self, new_key: bytes):
        """
        Rotate to a new key
        
        Args:
            new_key: New shared secret key
        """
        old_key_age = time.time() - self.key_created_at
        print(f"[MAC] Key rotation: old key was {old_key_age:.0f}s old")
        
        self.key = new_key
        self.key_created_at = time.time()
        
        # Clear replay protection state (old counters invalid with new key)
        self.last_counters.clear()
        self.message_timestamps.clear()
    
    def should_rotate_key(self) -> bool:
        """Check if key should be rotated"""
        age = time.time() - self.key_created_at
        return age >= self.key_rotation_interval
    
    def get_statistics(self) -> Dict:
        """Get authentication statistics"""
        return {
            'generated': self.stats['generated'],
            'verified': self.stats['verified'],
            'failed': self.stats['failed'],
            'replays': self.stats['replays'],
            'freshness_violations': self.stats['freshness_violations'],
            'success_rate': (
                self.stats['verified'] / max(1, self.stats['verified'] + self.stats['failed'])
            ) * 100,
            'key_age': time.time() - self.key_created_at
        }
    
    def reset_statistics(self):
        """Reset statistics counters"""
        self.stats = {k: 0 for k in self.stats}


class PowerManagementMAC:
    """
    Specialized MAC handler for Power Management messages
    
    Integrates MAC authentication into PM heartbeat and profile messages
    """
    
    def __init__(self, key: bytes = DEFAULT_KEY):
        """
        Initialize PM MAC handler
        
        Args:
            key: Shared secret key
        """
        self.authenticator = MACAuthenticator(key=key, truncate=True)
        
    def sign_heartbeat(self, header: bytes, length: bytes, payload: bytes) -> bytes:
        """
        Sign a heartbeat message with MAC
        
        Args:
            header: 4-byte PM header (0xFFFE8FFE)
            length: 4-byte length field
            payload: Message payload
            
        Returns:
            Complete message with MAC tag appended
        """
        # Message structure: header + length + payload + timestamp + MAC
        timestamp = struct.pack("!d", time.time())
        message_data = header + length + payload + timestamp
        
        # Generate MAC over entire message
        mac_tag = self.authenticator.generate_mac(message_data, include_timestamp=False)
        
        # Return: header + length + payload + timestamp + MAC
        return message_data + mac_tag
    
    def verify_heartbeat(self, message: bytes) -> Tuple[bytes, int, str]:
        """
        Verify and parse a heartbeat message
        
        Args:
            message: Complete message with MAC
            
        Returns:
            Tuple of (payload, counter, source_ip)
            
        Raises:
            MACVerificationFailed: If MAC invalid
            ReplayDetected: If replay detected
            FreshnessViolation: If message too old
        """
        if len(message) < 24 + 8 + 16:  # min: 24 (heartbeat) + 8 (timestamp) + 16 (MAC)
            raise ValueError(f"Message too short: {len(message)} bytes")
        
        # Split message
        mac_tag = message[-16:]  # Last 16 bytes
        timestamp_bytes = message[-24:-16]  # 8 bytes before MAC
        header_and_payload = message[:-24]  # Everything before timestamp
        
        # Verify MAC
        message_to_verify = message[:-16]  # Everything except MAC tag
        self.authenticator.verify_mac(message_to_verify, mac_tag, check_freshness=False)
        
        # Check freshness manually
        timestamp = struct.unpack("!d", timestamp_bytes)[0]
        age = time.time() - timestamp
        if age > FRESHNESS_TIMEOUT:
            raise FreshnessViolation(f"Message too old: {age:.2f}s")
        
        # Parse heartbeat fields
        header = struct.unpack("!I", header_and_payload[0:4])[0]
        length = struct.unpack("!I", header_and_payload[4:8])[0]
        
        # Extract source IP and counter from payload
        # Payload structure: 8 bytes SOME/IP + 4 bytes IP + 4 bytes counter
        payload = header_and_payload[8:]
        if len(payload) >= 16:
            src_ip_bytes = payload[8:12]
            counter_bytes = payload[12:16]
            
            src_ip = '.'.join(str(b) for b in src_ip_bytes)
            counter = struct.unpack("!I", counter_bytes)[0]
            
            # Check for replay
            self.authenticator.check_replay(src_ip, counter)
            
            return payload, counter, src_ip
        
        raise ValueError("Invalid heartbeat payload")
    
    def sign_profile_request(self, header: bytes, length: bytes, payload: bytes) -> bytes:
        """
        Sign a profile request message with MAC
        
        Args:
            header: 4-byte PM header (0xFFFD8FFF)
            length: 4-byte length field
            payload: Message payload
            
        Returns:
            Complete message with MAC tag appended
        """
        # Same structure as heartbeat
        timestamp = struct.pack("!d", time.time())
        message_data = header + length + payload + timestamp
        mac_tag = self.authenticator.generate_mac(message_data, include_timestamp=False)
        return message_data + mac_tag
    
    def verify_profile_request(self, message: bytes) -> Tuple[bytes, int, str, str]:
        """
        Verify and parse a profile request message
        
        Args:
            message: Complete message with MAC
            
        Returns:
            Tuple of (payload, profile_id, source_ip, dest_ip)
            
        Raises:
            MACVerificationFailed: If MAC invalid
        """
        if len(message) < 34 + 8 + 16:  # min: 34 (profile) + 8 (timestamp) + 16 (MAC)
            raise ValueError(f"Message too short: {len(message)} bytes")
        
        # Split message
        mac_tag = message[-16:]
        timestamp_bytes = message[-24:-16]
        header_and_payload = message[:-24]
        
        # Verify MAC
        message_to_verify = message[:-16]
        self.authenticator.verify_mac(message_to_verify, mac_tag, check_freshness=False)
        
        # Check freshness
        timestamp = struct.unpack("!d", timestamp_bytes)[0]
        age = time.time() - timestamp
        if age > FRESHNESS_TIMEOUT:
            raise FreshnessViolation(f"Message too old: {age:.2f}s")
        
        # Parse profile request fields
        payload = header_and_payload[8:]
        if len(payload) >= 26:
            src_ip_bytes = payload[8:12]
            dst_ip_bytes = payload[12:16]
            profile_id_bytes = payload[18:23]
            
            src_ip = '.'.join(str(b) for b in src_ip_bytes)
            dst_ip = '.'.join(str(b) for b in dst_ip_bytes)
            
            # Profile ID is 40 bits (5 bytes)
            profile_id = int.from_bytes(profile_id_bytes, byteorder='big')
            
            return payload, profile_id, src_ip, dst_ip
        
        raise ValueError("Invalid profile request payload")
    
    def get_statistics(self) -> Dict:
        """Get MAC statistics"""
        return self.authenticator.get_statistics()


def demonstrate_mac_authentication():
    """Demonstration of MAC authentication"""
    print("="*80)
    print("MAC Authentication Demonstration")
    print("="*80)
    
    # Create authenticator
    mac_handler = PowerManagementMAC()
    
    print("\n1. Creating MAC-authenticated Heartbeat Message")
    print("-" * 80)
    
    # Build heartbeat components
    PM_HEADER_HEARTBEAT = 0xFFFE8FFE
    header = struct.pack("!I", PM_HEADER_HEARTBEAT)
    
    # Payload: SOME/IP part + IP + counter
    someip_part = struct.pack("!IBBBB", 0x00000000, 0x01, 0x01, 0x02, 0x00)
    src_ip = struct.pack("!BBBB", 10, 0, 0, 1)
    counter = struct.pack("!I", 5)
    payload = someip_part + src_ip + counter
    
    length = struct.pack("!I", len(payload))
    
    # Sign the message
    signed_message = mac_handler.sign_heartbeat(header, length, payload)
    
    print(f"Original message: {len(header + length + payload)} bytes")
    print(f"Signed message:   {len(signed_message)} bytes")
    print(f"MAC tag length:   {len(signed_message) - len(header + length + payload) - 8} bytes")
    print(f"\nSigned message (hex):")
    print(' '.join(f'{b:02X}' for b in signed_message[:48]))
    print("... (MAC tag and timestamp)")
    
    print("\n2. Verifying MAC-authenticated Message")
    print("-" * 80)
    
    try:
        payload_verified, counter_val, source_ip = mac_handler.verify_heartbeat(signed_message)
        print(f"✅ MAC Verification: SUCCESS")
        print(f"   Source IP: {source_ip}")
        print(f"   Counter:   {counter_val}")
    except Exception as e:
        print(f"❌ MAC Verification: FAILED - {e}")
    
    print("\n3. Testing Replay Attack Protection")
    print("-" * 80)
    
    # Try to replay the same message
    try:
        time.sleep(0.1)  # Small delay
        payload_verified, counter_val, source_ip = mac_handler.verify_heartbeat(signed_message)
        print(f"⚠️  Replay check: Message accepted (same counter)")
    except ReplayDetected as e:
        print(f"✅ Replay Protection: BLOCKED - {e}")
    
    # Send message with older counter
    old_counter = struct.pack("!I", 3)
    payload_old = someip_part + src_ip + old_counter
    length_old = struct.pack("!I", len(payload_old))
    signed_old = mac_handler.sign_heartbeat(header, length_old, payload_old)
    
    try:
        payload_verified, counter_val, source_ip = mac_handler.verify_heartbeat(signed_old)
        print(f"⚠️  Old counter: Message accepted")
    except ReplayDetected as e:
        print(f"✅ Replay Protection: BLOCKED - {e}")
    
    print("\n4. Testing Message Tampering Detection")
    print("-" * 80)
    
    # Tamper with the message
    tampered_message = bytearray(signed_message)
    tampered_message[20] ^= 0xFF  # Flip bits in payload
    
    try:
        payload_verified, counter_val, source_ip = mac_handler.verify_heartbeat(bytes(tampered_message))
        print(f"❌ Tampering Detection: FAILED - tampered message accepted!")
    except MACVerificationFailed as e:
        print(f"✅ Tampering Detection: SUCCESS - {e}")
    
    print("\n5. Testing Freshness Validation")
    print("-" * 80)
    
    # Create message with old timestamp
    old_timestamp = struct.pack("!d", time.time() - 10.0)  # 10 seconds ago
    message_with_old_timestamp = header + length + payload + old_timestamp
    mac_tag = mac_handler.authenticator.generate_mac(message_with_old_timestamp, include_timestamp=False)
    old_message = message_with_old_timestamp + mac_tag
    
    try:
        payload_verified, counter_val, source_ip = mac_handler.verify_heartbeat(old_message)
        print(f"⚠️  Freshness Check: Old message accepted")
    except FreshnessViolation as e:
        print(f"✅ Freshness Validation: SUCCESS - {e}")
    
    print("\n6. Statistics")
    print("-" * 80)
    stats = mac_handler.get_statistics()
    print(f"Messages Generated: {stats['generated']}")
    print(f"Messages Verified:  {stats['verified']}")
    print(f"Verification Failed: {stats['failed']}")
    print(f"Replays Detected:   {stats['replays']}")
    print(f"Freshness Violations: {stats['freshness_violations']}")
    print(f"Success Rate:       {stats['success_rate']:.1f}%")
    print(f"Key Age:            {stats['key_age']:.1f} seconds")
    
    print("\n" + "="*80)
    print("✅ MAC Authentication Demonstration Complete!")
    print("="*80)


if __name__ == "__main__":
    # Run demonstration
    demonstrate_mac_authentication()
