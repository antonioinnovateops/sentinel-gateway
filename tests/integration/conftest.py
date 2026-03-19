"""
SIL Test Fixtures — Sentinel Gateway Integration Tests

Manages MCU simulator + Linux gateway lifecycle for pytest.
Maps to Integration Test Plan (ITP-001) test environment.

Usage:
    docker-compose run --rm sil
    # or locally:
    pytest tests/integration/ -v
"""

import subprocess
import socket
import struct
import time
import os
import signal
import pytest

# Network config matching sentinel_types.h
LINUX_IP = "127.0.0.1"
CMD_PORT = 5000    # MCU listens for commands
TEL_PORT = 5001    # Gateway listens for telemetry
DIAG_PORT = 5002   # Gateway listens for diagnostics

HEADER_SIZE = 5    # 4-byte LE length + 1-byte type

# Message types from sentinel_types.h
MSG_SENSOR_DATA    = 0x01
MSG_HEALTH_STATUS  = 0x02
MSG_ACTUATOR_CMD   = 0x10
MSG_ACTUATOR_RSP   = 0x11
MSG_CONFIG_UPDATE  = 0x20
MSG_CONFIG_RSP     = 0x21
MSG_DIAG_REQ       = 0x30
MSG_DIAG_RSP       = 0x31
MSG_STATE_SYNC_REQ = 0x40
MSG_STATE_SYNC_RSP = 0x41


def wait_for_port(port, host=LINUX_IP, timeout=10):
    """Wait until a TCP port is accepting connections."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(1)
            s.connect((host, port))
            s.close()
            return True
        except (ConnectionRefusedError, OSError, socket.timeout):
            time.sleep(0.2)
    return False


def send_frame(sock, msg_type, payload=b""):
    """Send a wire-frame message: [4-byte LE length][1-byte type][payload]"""
    header = struct.pack("<IB", len(payload), msg_type)
    sock.sendall(header + payload)


def recv_frame(sock, timeout=3):
    """Receive a wire-frame message. Returns (msg_type, payload) or (None, None)."""
    sock.settimeout(timeout)
    try:
        header = b""
        while len(header) < HEADER_SIZE:
            chunk = sock.recv(HEADER_SIZE - len(header))
            if not chunk:
                return None, None
            header += chunk
        payload_len = struct.unpack("<I", header[:4])[0]
        msg_type = header[4]
        payload = b""
        while len(payload) < payload_len:
            chunk = sock.recv(payload_len - len(payload))
            if not chunk:
                return msg_type, payload
            payload += chunk
        return msg_type, payload
    except (socket.timeout, OSError):
        return None, None


def connect_tcp(port, host=LINUX_IP, timeout=2):
    """Create a connected TCP socket. Caller must close."""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((host, port))
    return s


class SILEnvironment:
    """Manages MCU simulator + gateway processes for SIL testing.

    Supports two modes:
    1. Docker (sil service): uses pre-built gateway from /workspace/artifacts/
    2. Local: builds gateway from source via cmake
    """

    def __init__(self, project_root):
        self.project_root = project_root
        self.build_dir = os.path.join(project_root, "build-sil")
        self.mcu_proc = None
        self.gw_proc = None
        self.mcu_output = ""
        self.gw_output = ""
        self.sim_bin = None
        self.gw_bin = None

    @staticmethod
    def _can_execute(path):
        """Check if a binary can actually run on this platform."""
        if not os.path.isfile(path) or not os.access(path, os.X_OK):
            return False
        try:
            r = subprocess.run([path, "--version"], capture_output=True, timeout=2)
            return True
        except (OSError, subprocess.TimeoutExpired):
            return False

    def _find_or_build_gateway(self):
        """Find a runnable gateway or build from source."""
        # Check if we already built one
        sil_built = os.path.join(self.build_dir, "sentinel-gw")
        if self._can_execute(sil_built):
            print(f"[SIL] Using previously built gateway: {sil_built}")
            return sil_built

        # Build natively from source (cross-compiled aarch64 binaries won't run on x86)
        print("[SIL] Building native x86 gateway from source...")
        os.makedirs(self.build_dir, exist_ok=True)
        r = subprocess.run(
            ["cmake", self.project_root,
             "-DBUILD_LINUX=ON", "-DBUILD_MCU=OFF",
             "-DBUILD_TESTS=OFF", "-DCMAKE_BUILD_TYPE=Debug"],
            cwd=self.build_dir, capture_output=True, text=True
        )
        assert r.returncode == 0, f"cmake configure failed: {r.stderr}"

        r = subprocess.run(
            ["make", "-j4", "sentinel-gw"],
            cwd=self.build_dir, capture_output=True, text=True
        )
        assert r.returncode == 0, f"gateway build failed: {r.stderr}"
        return os.path.join(self.build_dir, "sentinel-gw")

    def _build_mcu_sim(self):
        """Compile the MCU simulator from C source."""
        os.makedirs(self.build_dir, exist_ok=True)

        sim_src = os.path.join(self.project_root, "tests", "integration", "sil_mcu_sim.c")
        common_dir = os.path.join(self.project_root, "src", "common")
        wire_src = os.path.join(common_dir, "wire_frame.c")
        sim_bin = os.path.join(self.build_dir, "mcu-sim")

        # Check if already compiled and up-to-date
        if os.path.isfile(sim_bin):
            sim_mtime = os.path.getmtime(sim_src)
            bin_mtime = os.path.getmtime(sim_bin)
            if bin_mtime > sim_mtime:
                return sim_bin

        r = subprocess.run(
            ["gcc", "-o", sim_bin, sim_src, wire_src,
             "-I", common_dir,
             "-Wall", "-Wno-unused-parameter", "-Wno-missing-prototypes"],
            capture_output=True, text=True
        )
        assert r.returncode == 0, f"MCU sim build failed: {r.stderr}"
        return sim_bin

    def build(self):
        """Build all SIL binaries."""
        self.gw_bin = self._find_or_build_gateway()
        self.sim_bin = self._build_mcu_sim()
        print(f"[SIL] Gateway:  {self.gw_bin}")
        print(f"[SIL] MCU sim:  {self.sim_bin}")

    def start(self):
        """Start MCU simulator and gateway processes."""
        # MCU sim first (listens on 5000)
        self.mcu_proc = subprocess.Popen(
            [self.sim_bin],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        time.sleep(0.3)

        # Gateway second (connects to MCU on 5000, listens on 5001/5002)
        gw_env = os.environ.copy()
        gw_env["SENTINEL_MCU_HOST"] = "127.0.0.1"
        self.gw_proc = subprocess.Popen(
            [self.gw_bin],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            env=gw_env,
            cwd="/tmp"  # Writable dir for log files
        )

        # Only wait for diagnostic port (external interface).
        # Do NOT connect to 5000 or 5001 — those are internal channels
        # between gateway and MCU. Connecting disrupts their connections.
        assert wait_for_port(DIAG_PORT, timeout=10), \
            "Gateway diagnostics port 5002 did not become ready"

        # Give MCU sim time to connect to gateway telemetry port
        time.sleep(1)
        print("[SIL] Environment is up")

    def stop(self):
        """Stop all processes and capture output."""
        for proc in [self.gw_proc, self.mcu_proc]:
            if proc and proc.poll() is None:
                proc.send_signal(signal.SIGTERM)
                try:
                    proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()

        if self.mcu_proc and self.mcu_proc.stdout:
            self.mcu_output = self.mcu_proc.stdout.read().decode("utf-8", errors="replace")
        if self.gw_proc:
            if self.gw_proc.stdout:
                self.gw_output = self.gw_proc.stdout.read().decode("utf-8", errors="replace")
            if self.gw_proc.stderr:
                self.gw_stderr = self.gw_proc.stderr.read().decode("utf-8", errors="replace")
            else:
                self.gw_stderr = ""
        print(f"[SIL] MCU output:\n{self.mcu_output[-500:]}")
        print(f"[SIL] Gateway stdout:\n{self.gw_output[-500:]}")
        print(f"[SIL] Gateway stderr:\n{self.gw_stderr[-500:]}")


@pytest.fixture(scope="module")
def sil_env():
    """Module-scoped SIL environment.

    Builds binaries, starts MCU sim + gateway, yields the environment,
    then stops everything on teardown.
    """
    root = os.environ.get("PROJECT_ROOT")
    if not root:
        d = os.path.dirname(os.path.abspath(__file__))
        while d != "/":
            if os.path.exists(os.path.join(d, "CMakeLists.txt")):
                root = d
                break
            d = os.path.dirname(d)
    assert root, "Cannot find project root (set PROJECT_ROOT or run from repo)"

    env = SILEnvironment(root)
    env.build()
    env.start()
    yield env
    env.stop()
