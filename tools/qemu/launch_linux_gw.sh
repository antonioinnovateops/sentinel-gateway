#!/bin/bash
#
# Launch Linux gateway in QEMU user-mode emulation (aarch64)
#
# Usage: ./launch_linux_gw.sh [gateway-binary] [options]
#
# Options:
#   --mcu-host    MCU hostname/IP (default: 127.0.0.1)
#   --mcu-port    MCU command port (default: 15000, mapped from QEMU)
#   --background  Run in background
#
# We use qemu-aarch64-static for user-mode emulation of the aarch64
# Linux gateway binary. This runs the binary directly using the host
# kernel with CPU instruction translation.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/../.."

# Default paths
DEFAULT_GW="${PROJECT_ROOT}/build/linux/sentinel-gw"
GW_PATH="${1:-$DEFAULT_GW}"

# Check if binary exists
if [[ ! -f "$GW_PATH" ]]; then
    echo "Error: Gateway binary not found: $GW_PATH"
    echo "Build it with: docker compose run --rm build-linux"
    exit 1
fi

# Check if it's an aarch64 binary
FILE_TYPE=$(file "$GW_PATH" 2>/dev/null || echo "")
if [[ "$FILE_TYPE" == *"x86-64"* ]]; then
    echo "Note: Binary is x86-64, running natively (no QEMU needed)"
    QEMU_PREFIX=""
elif [[ "$FILE_TYPE" == *"aarch64"* ]] || [[ "$FILE_TYPE" == *"ARM aarch64"* ]]; then
    echo "Binary is aarch64, using qemu-aarch64-static"
    QEMU_PREFIX="qemu-aarch64-static"

    # Check if qemu-aarch64-static is available
    if ! command -v qemu-aarch64-static &> /dev/null; then
        echo "Error: qemu-aarch64-static not found"
        echo "Install with: apt-get install qemu-user-static"
        exit 1
    fi
else
    echo "Warning: Unknown binary type, attempting native execution"
    QEMU_PREFIX=""
fi

# Parse options
MCU_HOST="127.0.0.1"
MCU_PORT="15000"
BACKGROUND=false

shift 2>/dev/null || true

for arg in "$@"; do
    case $arg in
        --mcu-host=*)
            MCU_HOST="${arg#*=}"
            ;;
        --mcu-port=*)
            MCU_PORT="${arg#*=}"
            ;;
        --background)
            BACKGROUND=true
            ;;
        *)
            echo "Unknown option: $arg"
            ;;
    esac
done

# Environment variables for the gateway
# The gateway expects MCU at 192.168.7.2:5000, but in QEMU SIL
# we redirect to localhost:15000 (QEMU port forward)
export SENTINEL_MCU_HOST="${MCU_HOST}"
export SENTINEL_MCU_PORT="${MCU_PORT}"

echo "=== QEMU Linux Gateway Launch ==="
echo "Binary: $GW_PATH"
echo "MCU:    ${MCU_HOST}:${MCU_PORT}"
if [[ -n "$QEMU_PREFIX" ]]; then
    echo "QEMU:   $QEMU_PREFIX (user-mode)"
fi
echo ""

if $BACKGROUND; then
    if [[ -n "$QEMU_PREFIX" ]]; then
        $QEMU_PREFIX "$GW_PATH" &
    else
        "$GW_PATH" &
    fi
    echo "Gateway PID: $!"
else
    echo "Press Ctrl-C to stop"
    echo "================================"
    if [[ -n "$QEMU_PREFIX" ]]; then
        exec $QEMU_PREFIX "$GW_PATH"
    else
        exec "$GW_PATH"
    fi
fi
