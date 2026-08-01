#!/usr/bin/env bash
# ==============================================================================
# tri-pi-test.sh — Run Triangles on emulated Raspberry Pi variants via QEMU
# 
# Usage:
#   ./tri-pi-test.sh [pi-model] [tri-args...]
#
# Pi models supported (aarch64):
#   pi3       Pi 3B/3A+ (Cortex-A53, 64-bit)  — user-mode QEMU
#   pi4       Pi 4B (Cortex-A72, 64-bit)      — user-mode QEMU
#   pi5       Pi 5 (Cortex-A76, 64-bit)       — user-mode QEMU
#   pi3-full  Pi 3B — full system emulation (qemu-system-aarch64 -M raspi3b)
#
# Examples:
#   ./tri-pi-test.sh pi3 --version
#   ./tri-pi-test.sh pi4 -regtest -notor -recovery-mode=1 -printtoconsole
#   ./tri-pi-test.sh pi3-full   # boots a full Pi OS (needs rootfs image)
#
# The aarch64 tri binaries are cross-compiled on DNS2 and run under
# qemu-aarch64-static. This tests the ARM binary's correctness — ABI
# compatibility, library resolution, crypto operations, database access,
# and Tor integration — without needing physical Pi hardware.
#
# For full-system emulation (testing kernel/hardware/driver interaction),
# use pi3-full mode with a Raspberry Pi OS rootfs.
# ==============================================================================

set -euo pipefail

PI_MODEL="${1:-pi3}"
shift || true

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TRI_SRC="/root/triangles_v5"
TRI_AARCH64_BIN="${TRI_SRC}/build-aarch64/bin/trianglesd"
TRI_AARCH64_CLI="${TRI_SRC}/build-aarch64/bin/triangles-cli"
QEMU_USER="/usr/bin/qemu-aarch64-static"
QEMU_SYS="/usr/bin/qemu-system-aarch64"
ARM_SYSROOT="/usr/aarch64-linux-gnu"

# Verify binary exists
if [[ ! -f "$TRI_AARCH64_BIN" ]]; then
    echo "ERROR: aarch64 trianglesd not found at $TRI_AARCH64_BIN" >&2
    echo "Build it with: cd $TRI_SRC && cmake --build build-aarch64 --target trianglesd" >&2
    exit 1
fi

run_user_mode() {
    local binary="$1"
    shift
    local model_name="$1"
    shift
    
    echo "╔═══════════════════════════════════════════════════════════╗"
    echo "║  Triangles on Raspberry Pi ${model_name} (QEMU user-mode)  ║"
    echo "╚═══════════════════════════════════════════════════════════╝"
    echo ""
    echo "Binary:  $(file "$binary" | cut -d: -f2)"
    echo "QEMU:    $($QEMU_USER --version | head -1)"
    echo "Args:    $*"
    echo ""
    
    # QEMU user-mode runs the ARM binary with the host kernel but ARM user-space
    # -L sets the sysroot for dynamic linker/library resolution
    exec "$QEMU_USER" -L "$ARM_SYSROOT" "$binary" "$@"
}

run_full_system_pi3() {
    echo "╔═══════════════════════════════════════════════════════════╗"
    echo "║  Triangles on Raspberry Pi 3B (QEMU full-system)         ║"
    echo "╚═══════════════════════════════════════════════════════════╝"
    
    local IMG_DIR="${TRI_SRC}/pi-emulation/images"
    local KERNEL="${IMG_DIR}/kernel8.img"
    local DTB="${IMG_DIR}/bcm2710-rpi-3-b.dtb"
    local ROOTFS="${IMG_DIR}/raspios-trixie-arm64.img"
    local OVERLAY="/tmp/tri-pi3-overlay.qcow2"
    
    if [[ ! -f "$KERNEL" ]] || [[ ! -f "$ROOTFS" ]]; then
        echo "ERROR: Pi 3 full-system images not found in $IMG_DIR" >&2
        echo "" >&2
        echo "To set up full-system emulation:" >&2
        echo "  1. Download Raspberry Pi OS Lite (64-bit) from raspberrypi.com" >&2
        echo "  2. Extract kernel8.img from the boot partition" >&2
        echo "  3. Get the DTB: bcm2710-rpi-3-b.dtb from the boot partition" >&2
        echo "  4. Place all in: $IMG_DIR/" >&2
        echo "" >&2
        echo "User-mode testing (default) works without these files." >&2
        exit 1
    fi
    
    # Create overlay so we don't modify the base image
    qemu-img create -f qcow2 -b "$ROOTFS" "$OVERLAY" 2>/dev/null || true
    
    exec "$QEMU_SYS" \
        -M raspi3b \
        -kernel "$KERNEL" \
        -dtb "$DTB" \
        -drive "file=$OVERLAY,if=sd,format=qcow2" \
        -m 1G \
        -smp 4 \
        -nographic \
        -append "console=ttyAMA0 root=/dev/mmcblk0p2 rootwait rw quiet"
}

case "$PI_MODEL" in
    pi3|pi4|pi5)
        # All three use the same aarch64 binary — the binary is 
        # architecture-compatible across Cortex-A53/A72/A76.
        # The model name documents which hardware variant is being simulated.
        run_user_mode "$TRI_AARCH64_BIN" "$PI_MODEL (Cortex-A*)"
        "$@"
        ;;
    pi3-cli|pi4-cli|pi5-cli)
        run_user_mode "$TRI_AARCH64_CLI" "$PI_MODEL CLI" "$@"
        ;;
    pi3-full)
        run_full_system_pi3
        ;;
    *)
        echo "Unknown model: $PI_MODEL" >&2
        echo "Supported: pi3, pi4, pi5, pi3-cli, pi4-cli, pi5-cli, pi3-full" >&2
        exit 1
        ;;
esac
