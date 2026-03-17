#!/bin/bash
# @file setup_sil.sh
# @brief Set up the Software-in-the-Loop (SIL) environment
# @implements [SYS-066, SYS-067] Boot time requirements
#
# Prerequisites: QEMU 8.0+, ARM toolchain, protobuf compiler
# Usage: ./tools/scripts/setup_sil.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
QEMU_DIR="${PROJECT_ROOT}/tools/qemu"

echo "=== Sentinel Gateway SIL Setup ==="
echo "Project root: ${PROJECT_ROOT}"
echo ""

# ─────────────────────────────────────────────
# Check prerequisites
# ─────────────────────────────────────────────
echo "[1/6] Checking prerequisites..."

check_command() {
    if ! command -v "$1" &> /dev/null; then
        echo "ERROR: $1 not found. Install: $2"
        exit 1
    fi
    echo "  ✓ $1 found: $(command -v "$1")"
}

check_command "qemu-system-aarch64" "apt install qemu-system-arm"
check_command "qemu-system-arm" "apt install qemu-system-arm"
check_command "aarch64-linux-gnu-gcc" "apt install gcc-aarch64-linux-gnu"
check_command "arm-none-eabi-gcc" "apt install gcc-arm-none-eabi"
check_command "protoc" "apt install protobuf-compiler"
check_command "cmake" "apt install cmake"
check_command "make" "apt install build-essential"

echo ""

# ─────────────────────────────────────────────
# Generate protobuf code
# ─────────────────────────────────────────────
echo "[2/6] Generating protobuf code..."

PROTO_FILE="${PROJECT_ROOT}/src/proto/sentinel.proto"

# Linux (protobuf-c)
LINUX_PB_DIR="${PROJECT_ROOT}/src/linux/app/protobuf_handler"
mkdir -p "${LINUX_PB_DIR}"
if command -v protoc-gen-c &> /dev/null || command -v protoc-c &> /dev/null; then
    protoc --c_out="${LINUX_PB_DIR}" -I"${PROJECT_ROOT}/src/proto" sentinel.proto
    echo "  ✓ Linux protobuf-c generated"
else
    echo "  ⚠ protobuf-c not found — install with: apt install libprotobuf-c-dev protobuf-c-compiler"
fi

# MCU (nanopb)
MCU_PB_DIR="${PROJECT_ROOT}/src/mcu/app/protobuf_handler"
mkdir -p "${MCU_PB_DIR}"
if [ -d "${PROJECT_ROOT}/tools/nanopb" ]; then
    protoc --plugin=protoc-gen-nanopb="${PROJECT_ROOT}/tools/nanopb/generator/protoc-gen-nanopb" \
           --nanopb_out="${MCU_PB_DIR}" \
           -I"${PROJECT_ROOT}/src/proto" sentinel.proto
    echo "  ✓ MCU nanopb generated"
else
    echo "  ⚠ nanopb not found — clone: git clone https://github.com/nanopb/nanopb tools/nanopb"
fi

echo ""

# ─────────────────────────────────────────────
# Create QEMU network bridge
# ─────────────────────────────────────────────
echo "[3/6] Setting up QEMU network bridge..."

mkdir -p "${QEMU_DIR}"

cat > "${QEMU_DIR}/bridge.conf" << 'EOF'
# QEMU network bridge for SIL
# Creates a virtual network between Linux VM and MCU VM
# Linux: 192.168.7.1/24
# MCU:   192.168.7.2/24
EOF

echo "  ✓ Network bridge config created"
echo ""

# ─────────────────────────────────────────────
# Create QEMU launch scripts
# ─────────────────────────────────────────────
echo "[4/6] Creating QEMU launch scripts..."

# Linux VM launcher
cat > "${QEMU_DIR}/run_linux.sh" << 'SCRIPT'
#!/bin/bash
# Launch Linux gateway in QEMU
# Requires: Yocto image at tools/qemu/linux-image.ext4

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a53 \
    -m 512M \
    -kernel "${SCRIPT_DIR}/Image" \
    -drive file="${SCRIPT_DIR}/linux-image.ext4",format=raw,if=virtio \
    -append "root=/dev/vda rw console=ttyAMA0" \
    -netdev socket,id=net0,listen=:12345 \
    -device virtio-net-device,netdev=net0 \
    -nographic \
    -monitor unix:"${SCRIPT_DIR}/linux-monitor.sock",server,nowait
SCRIPT
chmod +x "${QEMU_DIR}/run_linux.sh"

# MCU VM launcher
cat > "${QEMU_DIR}/run_mcu.sh" << 'SCRIPT'
#!/bin/bash
# Launch MCU firmware in QEMU
# Requires: firmware.elf at build/mcu/sentinel-mcu.elf

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

qemu-system-arm \
    -machine netduinoplus2 \
    -cpu cortex-m4 \
    -m 1M \
    -kernel "${PROJECT_ROOT}/build/mcu/sentinel-mcu.elf" \
    -netdev socket,id=net0,connect=:12345 \
    -device virtio-net-device,netdev=net0 \
    -nographic \
    -semihosting-config enable=on,target=native \
    -monitor unix:"${SCRIPT_DIR}/mcu-monitor.sock",server,nowait
SCRIPT
chmod +x "${QEMU_DIR}/run_mcu.sh"

echo "  ✓ QEMU launch scripts created"
echo ""

# ─────────────────────────────────────────────
# Build project
# ─────────────────────────────────────────────
echo "[5/6] Building project..."

mkdir -p "${PROJECT_ROOT}/build"
cd "${PROJECT_ROOT}/build"

cmake .. -DBUILD_LINUX=ON -DBUILD_MCU=OFF -DBUILD_TESTS=ON
echo "  ✓ CMake configuration complete"
echo ""

# ─────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────
echo "[6/6] Setup complete!"
echo ""
echo "Next steps:"
echo "  1. Build Linux gateway:  cd build && make sentinel-gw"
echo "  2. Build MCU firmware:   cmake .. -DBUILD_MCU=ON && make sentinel-mcu"
echo "  3. Build Yocto image:    ./tools/yocto/build.sh"
echo "  4. Run SIL:              ./tools/scripts/run_sil.sh"
echo "  5. Run tests:            cd build && ctest --output-on-failure"
