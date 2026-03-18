#!/bin/bash
#
# Launch MCU firmware in QEMU system emulation (MPS2-AN505)
#
# Usage: ./launch_mcu_fw.sh [firmware.elf] [options]
#
# Options:
#   --gdb         Start with GDB server on port 1234
#   --serial      Enable serial output to stdout
#   --netdev      Configure network device (default: user with port forwarding)
#
# The MPS2-AN505 is a Cortex-M33 FPGA image that QEMU supports.
# We use qemu-system-arm -M mps2-an505 for full system emulation.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/../.."

# Default paths
DEFAULT_FW="${PROJECT_ROOT}/build/qemu-mcu/sentinel-mcu-qemu.elf"
FW_PATH="${1:-$DEFAULT_FW}"

# Check if firmware exists
if [[ ! -f "$FW_PATH" ]]; then
    echo "Error: Firmware not found: $FW_PATH"
    echo "Build it with: cmake -B build/qemu-mcu -DBUILD_QEMU_MCU=ON && cmake --build build/qemu-mcu"
    exit 1
fi

# Parse options
GDB_OPT=""
SERIAL_OPT="-serial null"
NETDEV_OPT=""
BACKGROUND=false

for arg in "${@:2}"; do
    case $arg in
        --gdb)
            GDB_OPT="-s -S"
            echo "GDB server enabled on port 1234 (waiting for connection)"
            ;;
        --serial)
            SERIAL_OPT="-serial stdio"
            ;;
        --background)
            BACKGROUND=true
            ;;
        *)
            echo "Unknown option: $arg"
            ;;
    esac
done

# Network configuration:
# - MCU listens on port 5000 (command) - forward to host 15000
# - MCU connects to Linux gateway on port 5001 (telemetry)
# User-mode networking with port forwarding
NETDEV_OPT="-netdev user,id=net0,hostfwd=tcp::15000-:5000 -device lan9118,netdev=net0"

# QEMU command
QEMU_CMD=(
    qemu-system-arm
    -M mps2-an505
    -cpu cortex-m33
    -m 4M
    -kernel "$FW_PATH"
    -nographic
    $SERIAL_OPT
    $NETDEV_OPT
    $GDB_OPT
)

echo "=== QEMU MCU Firmware Launch ==="
echo "Machine:  MPS2-AN505 (Cortex-M33)"
echo "Firmware: $FW_PATH"
echo "Network:  User mode, TCP 5000->15000"
echo ""

if $BACKGROUND; then
    "${QEMU_CMD[@]}" &
    echo "QEMU PID: $!"
else
    echo "Press Ctrl-A X to exit QEMU"
    echo "================================"
    exec "${QEMU_CMD[@]}"
fi
