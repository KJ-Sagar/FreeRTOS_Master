#!/bin/bash
#
# Run FreeRTOS TCP Echo demo on QEMU (MPS2)
# Build already done
#
# Usage:
#   sudo ./run_tcp_echo_qemu.sh

set -e

### CONFIG ###
TAP_IF=tap0
HOST_IP=10.0.0.1/24
ECHO_PORT_LETTERS=7
ECHO_PORT=5001 #ECHO_PORT_HEARTBEAT
ELF=./build/freertos_tcp_mps2_demo.axf
################

echo "=== FreeRTOS TCP Echo QEMU Runner ==="

### Detect firewall ###
FIREWALL=""
if command -v ufw >/dev/null 2>&1; then
    FIREWALL="ufw"
elif command -v firewall-cmd >/dev/null 2>&1; then
    FIREWALL="firewalld"
fi

### Cleanup handler ###
cleanup() {
    echo
    echo "Cleaning up..."

    if [[ -n "$NC_PID" ]]; then
        kill $NC_PID 2>/dev/null || true
    fi

    ip link set $TAP_IF down 2>/dev/null || true

    if [[ "$FIREWALL" == "ufw" ]]; then
        echo "Re-enabling UFW firewall"
        ufw enable || true
    elif [[ "$FIREWALL" == "firewalld" ]]; then
        echo "Re-enabling firewalld"
        systemctl start firewalld || true
    fi

    echo "Done."
}
trap cleanup EXIT

### 1. Disable firewall ###
echo "[1/4] Disabling firewall (temporarily)"

if [[ "$FIREWALL" == "ufw" ]]; then
    ufw disable
elif [[ "$FIREWALL" == "firewalld" ]]; then
    systemctl stop firewalld
else
    echo "No supported firewall detected (skipping)"
fi

### 2. TAP interface setup ###
echo "[2/4] Setting up TAP interface: $TAP_IF"

ip tuntap add dev $TAP_IF mode tap user $(whoami) 2>/dev/null || true
ip addr flush dev $TAP_IF || true
ip addr add $HOST_IP dev $TAP_IF
ip link set $TAP_IF up

echo "TAP interface $TAP_IF up with IP $HOST_IP"

### 3. Start netcat echo server ###
echo "[3/4] Starting netcat echo server on port $ECHO_PORT"
nc -l -p $ECHO_PORT_LETTERS &
nc -l -p $ECHO_PORT &
NC_PID=$!

sleep 1

### 4. Run QEMU ###
echo "[4/4] Launching QEMU"
sudo qemu-system-arm \
    -machine mps2-an385 \
    -cpu cortex-m3 \
    -kernel $ELF \
    -netdev tap,id=mynet0,ifname=$TAP_IF,script=no,downscript=no \
    -net nic,model=lan9118,netdev=mynet0 \
    -serial stdio \
    -nographic \
    -monitor null \
    -semihosting \
    -semihosting-config enable=on,target=native