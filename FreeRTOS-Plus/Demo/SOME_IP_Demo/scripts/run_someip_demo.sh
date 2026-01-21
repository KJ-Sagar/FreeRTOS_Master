#!/bin/bash
set -e

# ================= CONFIG =================
TAP_IF=tap0
HOST_IP=10.0.0.1/24
ELF=../build/freertos_tcp_mps2_demo.axf
QEMU_LOG=/tmp/qemu_someip.log
CLIENT_SCRIPT=../linux/someip_UDP.py
# ==========================================

echo "=== SOME/IP FULL DEMO LAUNCHER ==="

cleanup() {
    echo
    echo "[CLEANUP] Shutting down..."

    if [[ -n "$QEMU_PID" ]]; then
        sudo kill $QEMU_PID 2>/dev/null || true
    fi

    ip link set $TAP_IF down 2>/dev/null || true

    echo "[CLEANUP] Done."
}
trap cleanup EXIT

# -------------------------------------------------
# 1. TAP setup
# -------------------------------------------------
echo "[1/5] Setting up TAP interface"

sudo ip tuntap add dev $TAP_IF mode tap user $(whoami) 2>/dev/null || true
sudo ip addr flush dev $TAP_IF || true
sudo ip addr add $HOST_IP dev $TAP_IF
sudo ip link set $TAP_IF up

# -------------------------------------------------
# 2. Launch QEMU (server) in background
# -------------------------------------------------
echo "[2/5] Launching QEMU SOME/IP server"

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
    -semihosting-config enable=on,target=native \
    > $QEMU_LOG 2>&1 &

QEMU_PID=$!
echo "QEMU PID = $QEMU_PID"

# -------------------------------------------------
# 3. Wait for server readiness
# -------------------------------------------------
echo "[3/5] Waiting for SOME/IP server to be ready..."

READY=0
for i in {1..20}; do
    if grep -q "SOME/IP: Listening on" $QEMU_LOG; then
        READY=1
        break
    fi
    sleep 1
done

if [[ $READY -ne 1 ]]; then
    echo "ERROR: SOME/IP server did not start"
    echo "---- QEMU LOG ----"
    cat $QEMU_LOG
    exit 1
fi

echo "Server is ready."

# -------------------------------------------------
# 4. Launch Linux client
# -------------------------------------------------
echo "[4/5] Launching SOME/IP client"
sleep 2
python3 $CLIENT_SCRIPT

# -------------------------------------------------
# 5. Done
# -------------------------------------------------
echo "[5/5] Demo finished"
