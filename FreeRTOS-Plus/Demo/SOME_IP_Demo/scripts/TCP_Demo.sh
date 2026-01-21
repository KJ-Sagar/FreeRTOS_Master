#!/bin/bash
#
# Run FreeRTOS SOME/IP Demo on QEMU (MPS2)
#
# QEMU hosts:
#   - FreeRTOS
#   - FreeRTOS+TCP
#   - SOME/IP server (TCP)
#
# Linux host provides:
#   - TAP networking only
#
# Usage:
#   sudo ./TCP_Demo.sh
#

set -e

### CONFIG ###
TAP_IF=tap0
HOST_IP=10.0.0.1/24
ELF=../build/freertos_tcp_mps2_demo.axf
################

echo "=== FreeRTOS SOME/IP Demo (QEMU + TAP) ==="

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
echo "[1/3] Disabling firewall (temporarily)"

if [[ "$FIREWALL" == "ufw" ]]; then
    ufw disable
elif [[ "$FIREWALL" == "firewalld" ]]; then
    systemctl stop firewalld
else
    echo "No supported firewall detected (skipping)"
fi

### 2. TAP interface setup ###
echo "[2/3] Setting up TAP interface: $TAP_IF"

ip tuntap add dev $TAP_IF mode tap user $(whoami) 2>/dev/null || true
ip addr flush dev $TAP_IF || true
ip addr add $HOST_IP dev $TAP_IF
ip link set $TAP_IF up

echo "TAP interface $TAP_IF up with IP $HOST_IP"

### 3. Run QEMU ###
echo "[3/3] Launching QEMU (FreeRTOS SOME/IP server)"

sudo qemu-system-arm \
    -machine mps2-an385 \
    -cpu cortex-m3 \
    -kernel $ELF \
    -netdev tap,id=net0,ifname=$TAP_IF,script=no,downscript=no \
    -net nic,model=lan9118,netdev=net0 \
    -serial stdio \
    -nographic \
    -monitor null \
    -semihosting \
    -semihosting-config enable=on,target=native
