#!/bin/bash
#
# Run FreeRTOS TCP Demo on QEMU (MPS2)
# QEMU hosts the TCP application (Echo / Heartbeat)
# Linux provides TAP networking only
#
# Usage:
#   sudo ./TCP_Demo.sh

set -e

### CONFIG ###
TAP_IF=tap0
HOST_IP=10.0.0.1/24
ELF=../build/freertos_tcp_mps2_demo.axf
################

echo "=== FreeRTOS TCP Demo (QEMU + TAP) ==="

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

if grep -q "APP_DEMO_HEARTBEAT 1" ../app/app_config.h; then
    echo "[3/4] Starting Linux heartbeat server"
    nc -l -p 5001 &
    NC_PID=$!
else
    echo "[3/4] Linux ready (QEMU hosts TCP server)"
fi


### 4. Run QEMU ###
echo "[4/4] Launching QEMU (FreeRTOS TCP application)"
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
