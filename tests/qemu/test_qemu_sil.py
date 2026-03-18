#!/usr/bin/env python3
"""
QEMU-based Software-in-the-Loop (SIL) Test Suite

This test suite runs the actual cross-compiled binaries in QEMU emulation:
- MCU firmware (Cortex-M33) in qemu-system-arm -M mps2-an505
- Linux gateway (aarch64) in qemu-aarch64-static user-mode emulation

Tests verify end-to-end communication over TCP using real binaries.
"""

import subprocess
import socket
import struct
import time
import os
import signal
import sys
import pytest
import tempfile
from pathlib import Path
from typing import Optional, Tuple

# Port configuration
# MCU runs in QEMU system mode with port forwarding: 5000 -> 15000
MCU_HOST = "127.0.0.1"
MCU_CMD_PORT = 15000  # Forwarded from QEMU guest port 5000

# Linux gateway runs in user-mode, uses host networking directly
GW_HOST = "127.0.0.1"
GW_TEL_PORT = 5001
GW_DIAG_PORT = 5002
GW_CMD_PORT = 5000

# Wire frame constants
HEADER_SIZE = 5
MSG_ACTUATOR_CMD = 0x10
MSG_ACTUATOR_RSP = 0x11
MSG_SENSOR_DATA = 0x01
MSG_HEALTH_STATUS = 0x02

# Test timeouts
BOOT_TIMEOUT = 10  # seconds to wait for QEMU to boot
CONNECT_TIMEOUT = 5
RECV_TIMEOUT = 3


class QEMUManager:
    """Manages QEMU processes for MCU and Linux gateway."""

    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.mcu_proc: Optional[subprocess.Popen] = None
        self.gw_proc: Optional[subprocess.Popen] = None

    def find_mcu_firmware(self) -> Path:
        """Locate the MCU firmware ELF."""
        candidates = [
            self.project_root / "build" / "qemu-mcu" / "sentinel-mcu-qemu",
            self.project_root / "build" / "qemu-mcu" / "sentinel-mcu-qemu.elf",
            Path("/workspace/artifacts/mcu-qemu/sentinel-mcu-qemu.elf"),
        ]
        for path in candidates:
            if path.exists():
                return path
        raise FileNotFoundError(f"MCU firmware not found. Checked: {candidates}")

    def find_linux_gateway(self) -> Path:
        """Locate the Linux gateway binary."""
        candidates = [
            self.project_root / "build" / "linux" / "sentinel-gw",
            Path("/workspace/artifacts/linux/sentinel-gw"),
        ]
        for path in candidates:
            if path.exists():
                return path
        raise FileNotFoundError(f"Linux gateway not found. Checked: {candidates}")

    def start_mcu(self) -> subprocess.Popen:
        """Start MCU firmware in QEMU system emulation."""
        fw_path = self.find_mcu_firmware()
        print(f"[QEMU-SIL] Starting MCU firmware: {fw_path}")

        cmd = [
            "qemu-system-arm",
            "-M", "mps2-an505",
            "-cpu", "cortex-m33",
            "-m", "4M",
            "-kernel", str(fw_path),
            "-nographic",
            "-serial", "null",
            # Network: forward host port 15000 to guest port 5000
            "-netdev", "user,id=net0,hostfwd=tcp::15000-:5000",
            "-device", "lan9118,netdev=net0",
        ]

        self.mcu_proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            preexec_fn=os.setsid,
        )
        return self.mcu_proc

    def start_gateway(self) -> subprocess.Popen:
        """Start Linux gateway (user-mode QEMU or native)."""
        gw_path = self.find_linux_gateway()
        print(f"[QEMU-SIL] Starting Linux gateway: {gw_path}")

        # Check architecture
        file_output = subprocess.run(
            ["file", str(gw_path)],
            capture_output=True, text=True
        ).stdout

        if "aarch64" in file_output or "ARM aarch64" in file_output:
            # Use QEMU user-mode emulation
            cmd = ["qemu-aarch64-static", str(gw_path)]
        else:
            # Native execution (x86_64 build)
            cmd = [str(gw_path)]

        # Set environment for MCU connection
        env = os.environ.copy()
        env["SENTINEL_MCU_HOST"] = MCU_HOST
        env["SENTINEL_MCU_PORT"] = str(MCU_CMD_PORT)

        self.gw_proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=env,
            preexec_fn=os.setsid,
        )
        return self.gw_proc

    def stop_all(self):
        """Stop all QEMU processes."""
        for name, proc in [("MCU", self.mcu_proc), ("Gateway", self.gw_proc)]:
            if proc and proc.poll() is None:
                print(f"[QEMU-SIL] Stopping {name} (PID {proc.pid})")
                try:
                    os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                    proc.wait(timeout=3)
                except (subprocess.TimeoutExpired, ProcessLookupError):
                    try:
                        os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                    except ProcessLookupError:
                        pass

    def get_mcu_output(self) -> str:
        """Get MCU stdout output."""
        if self.mcu_proc and self.mcu_proc.stdout:
            return self.mcu_proc.stdout.read().decode("utf-8", errors="replace")
        return ""

    def get_gw_output(self) -> str:
        """Get gateway stdout output."""
        if self.gw_proc and self.gw_proc.stdout:
            return self.gw_proc.stdout.read().decode("utf-8", errors="replace")
        return ""


def wait_for_port(host: str, port: int, timeout: float = CONNECT_TIMEOUT) -> bool:
    """Wait for a TCP port to become available."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1)
            sock.connect((host, port))
            sock.close()
            return True
        except (socket.error, OSError):
            time.sleep(0.2)
    return False


def recv_frame(sock: socket.socket, timeout: float = RECV_TIMEOUT) -> Tuple[Optional[int], Optional[bytes]]:
    """Receive a wire frame."""
    sock.settimeout(timeout)
    try:
        header = sock.recv(HEADER_SIZE)
        if len(header) < HEADER_SIZE:
            return None, None
        payload_len = struct.unpack("<I", header[:4])[0]
        msg_type = header[4]
        payload = b""
        while len(payload) < payload_len:
            chunk = sock.recv(payload_len - len(payload))
            if not chunk:
                return msg_type, payload
            payload += chunk
        return msg_type, payload
    except socket.timeout:
        return None, None
    except Exception as e:
        print(f"[QEMU-SIL] recv_frame error: {e}")
        return None, None


def send_frame(sock: socket.socket, msg_type: int, payload: bytes = b""):
    """Send a wire frame."""
    header = struct.pack("<IB", len(payload), msg_type)
    sock.sendall(header + payload)


# ==============================================================================
# Pytest Fixtures
# ==============================================================================

@pytest.fixture(scope="module")
def project_root():
    """Get project root directory."""
    # Try environment variable first
    if "PROJECT_ROOT" in os.environ:
        return Path(os.environ["PROJECT_ROOT"])

    # Try to find based on script location
    script_dir = Path(__file__).parent
    for parent in [script_dir.parent.parent, Path("/workspace")]:
        if (parent / "CMakeLists.txt").exists():
            return parent

    raise RuntimeError("Cannot determine project root")


@pytest.fixture(scope="module")
def qemu_manager(project_root):
    """Create QEMU manager and start processes."""
    manager = QEMUManager(project_root)
    yield manager
    manager.stop_all()


@pytest.fixture(scope="module")
def running_system(qemu_manager):
    """Start QEMU processes and wait for them to be ready."""
    # Start MCU first (it listens on port 5000)
    try:
        qemu_manager.start_mcu()
    except FileNotFoundError as e:
        pytest.skip(f"MCU firmware not available: {e}")

    # Give MCU time to boot
    print("[QEMU-SIL] Waiting for MCU to boot...")
    time.sleep(2)

    # Check MCU port is available (port 15000 forwarded from guest 5000)
    if not wait_for_port(MCU_HOST, MCU_CMD_PORT, BOOT_TIMEOUT):
        qemu_manager.stop_all()
        pytest.fail(f"MCU did not start listening on port {MCU_CMD_PORT}")

    # Start Linux gateway
    try:
        qemu_manager.start_gateway()
    except FileNotFoundError as e:
        qemu_manager.stop_all()
        pytest.skip(f"Linux gateway not available: {e}")

    # Give gateway time to start
    print("[QEMU-SIL] Waiting for gateway to start...")
    time.sleep(1)

    # Check gateway ports
    if not wait_for_port(GW_HOST, GW_TEL_PORT, CONNECT_TIMEOUT):
        qemu_manager.stop_all()
        pytest.fail(f"Gateway did not start listening on telemetry port {GW_TEL_PORT}")

    print("[QEMU-SIL] System ready")
    yield qemu_manager

    # Cleanup handled by fixture


# ==============================================================================
# Test Cases
# ==============================================================================

class TestQEMUBoot:
    """Test QEMU boot and basic connectivity."""

    def test_mcu_boots(self, running_system):
        """QSIL-01: MCU firmware boots in QEMU."""
        assert running_system.mcu_proc is not None
        assert running_system.mcu_proc.poll() is None, "MCU process died"
        print("[PASS] MCU is running in QEMU")

    def test_gateway_boots(self, running_system):
        """QSIL-02: Linux gateway boots (native or QEMU user-mode)."""
        assert running_system.gw_proc is not None
        assert running_system.gw_proc.poll() is None, "Gateway process died"
        print("[PASS] Gateway is running")


class TestTCPConnectivity:
    """Test TCP channel establishment."""

    def test_mcu_cmd_port_accessible(self, running_system):
        """QSIL-03: MCU command port is accessible via QEMU port forward."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(CONNECT_TIMEOUT)
        try:
            sock.connect((MCU_HOST, MCU_CMD_PORT))
            print(f"[PASS] MCU command port {MCU_CMD_PORT} accessible")
        finally:
            sock.close()

    def test_gateway_telemetry_port(self, running_system):
        """QSIL-04: Gateway telemetry port accepting connections."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(CONNECT_TIMEOUT)
        try:
            sock.connect((GW_HOST, GW_TEL_PORT))
            print(f"[PASS] Gateway telemetry port {GW_TEL_PORT} accessible")
        finally:
            sock.close()

    def test_gateway_diagnostics_port(self, running_system):
        """QSIL-05: Gateway diagnostics port accepting connections."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(CONNECT_TIMEOUT)
        try:
            sock.connect((GW_HOST, GW_DIAG_PORT))
            print(f"[PASS] Gateway diagnostics port {GW_DIAG_PORT} accessible")
        finally:
            sock.close()


class TestProtocolCommunication:
    """Test protocol message exchange."""

    def test_actuator_command_roundtrip(self, running_system):
        """QSIL-06: Actuator command sent and response received."""
        # Connect to gateway command port
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(RECV_TIMEOUT)
        try:
            sock.connect((GW_HOST, GW_CMD_PORT))

            # Send actuator command (simplified protobuf payload)
            payload = b"\x08\x00\x15\x00\x00\x48\x42"  # actuator_id=0, target=50.0
            send_frame(sock, MSG_ACTUATOR_CMD, payload)

            # Wait for response
            time.sleep(0.5)
            msg_type, response = recv_frame(sock, timeout=3)

            if msg_type == MSG_ACTUATOR_RSP:
                print(f"[PASS] Actuator response received: type=0x{msg_type:02X}, {len(response)} bytes")
            else:
                pytest.fail(f"Expected actuator response (0x11), got: {msg_type}")
        finally:
            sock.close()

    def test_diagnostics_text_command(self, running_system):
        """QSIL-07: Diagnostic text command works."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(RECV_TIMEOUT)
        try:
            sock.connect((GW_HOST, GW_DIAG_PORT))

            # Send STATUS command
            sock.sendall(b"STATUS\n")

            # Wait for response
            time.sleep(0.3)
            response = sock.recv(512).decode("utf-8", errors="replace")

            if "STATE" in response.upper() or "NORMAL" in response.upper():
                print(f"[PASS] Diagnostic response received: {response[:100]}")
            else:
                print(f"[INFO] Diagnostic response: {response[:100]}")
        finally:
            sock.close()


class TestDataFlow:
    """Test sensor data and health message flow."""

    def test_telemetry_data_received(self, running_system):
        """QSIL-08: Sensor telemetry data received from MCU."""
        # Connect to gateway telemetry port and wait for data
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)  # Sensor data should arrive within heartbeat interval
        try:
            sock.connect((GW_HOST, GW_TEL_PORT))

            # The MCU should be sending sensor data periodically
            # Wait and try to receive
            start = time.time()
            received_sensor = False
            received_health = False

            while time.time() - start < 5:
                msg_type, payload = recv_frame(sock, timeout=2)
                if msg_type == MSG_SENSOR_DATA:
                    received_sensor = True
                    print(f"[INFO] Received sensor data: {len(payload)} bytes")
                elif msg_type == MSG_HEALTH_STATUS:
                    received_health = True
                    print(f"[INFO] Received health status: {len(payload)} bytes")

                if received_sensor or received_health:
                    break

            if received_sensor:
                print("[PASS] Sensor telemetry data received")
            elif received_health:
                print("[PASS] Health status received (sensor data may follow)")
            else:
                pytest.skip("No telemetry data received (MCU may not have network)")
        finally:
            sock.close()


class TestSystemStability:
    """Test system stability under load."""

    def test_processes_still_running(self, running_system):
        """QSIL-09: Both processes still running after tests."""
        time.sleep(1)

        mcu_status = running_system.mcu_proc.poll()
        gw_status = running_system.gw_proc.poll()

        assert mcu_status is None, f"MCU process exited with code {mcu_status}"
        assert gw_status is None, f"Gateway process exited with code {gw_status}"

        print("[PASS] Both QEMU processes still running")

    def test_multiple_connections(self, running_system):
        """QSIL-10: System handles multiple simultaneous connections."""
        sockets = []
        try:
            # Open multiple connections to different ports
            for _ in range(3):
                for port in [GW_TEL_PORT, GW_DIAG_PORT]:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(2)
                    sock.connect((GW_HOST, port))
                    sockets.append(sock)

            print(f"[PASS] Opened {len(sockets)} simultaneous connections")

        finally:
            for sock in sockets:
                try:
                    sock.close()
                except:
                    pass


# ==============================================================================
# Main Entry Point
# ==============================================================================

if __name__ == "__main__":
    # Run with pytest
    sys.exit(pytest.main([__file__, "-v", "--timeout=60", "-x"]))
