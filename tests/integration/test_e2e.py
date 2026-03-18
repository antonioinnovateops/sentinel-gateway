#!/usr/bin/env python3
"""
End-to-end integration tests for Sentinel Gateway SIL.
Starts MCU simulator + Linux gateway, verifies communication.
"""

import subprocess
import socket
import struct
import time
import os
import signal
import sys

LINUX_IP = "127.0.0.1"
CMD_PORT = 5000
TEL_PORT = 5001
DIAG_PORT = 5002

HEADER_SIZE = 5
MSG_ACTUATOR_CMD = 0x10
MSG_ACTUATOR_RSP = 0x11

passed = 0
failed = 0

def ok(name):
    global passed
    passed += 1
    print(f"  ✅ {name}")

def fail(name, reason=""):
    global failed
    failed += 1
    print(f"  ❌ {name}: {reason}")

def wait_for_port(port, timeout=5):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(1)
            s.connect((LINUX_IP, port))
            s.close()
            return True
        except:
            time.sleep(0.2)
    return False

def recv_frame(sock, timeout=3):
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
    except:
        return None, None

def send_frame(sock, msg_type, payload=b""):
    header = struct.pack("<IB", len(payload), msg_type)
    sock.sendall(header + payload)

def main():
    global passed, failed
    build_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "build-sil")
    src_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")

    print("=" * 60)
    print("Sentinel Gateway — End-to-End Integration Tests")
    print("=" * 60)

    # Build
    os.makedirs(build_dir, exist_ok=True)
    r = subprocess.run(
        ["cmake", src_dir, "-DBUILD_LINUX=ON", "-DBUILD_MCU=OFF",
         "-DBUILD_TESTS=OFF", "-DCMAKE_BUILD_TYPE=Debug"],
        cwd=build_dir, capture_output=True, text=True
    )
    assert r.returncode == 0, f"cmake failed: {r.stderr}"

    r = subprocess.run(["make", "-j4", "sentinel-gw"], cwd=build_dir, capture_output=True, text=True)
    assert r.returncode == 0, f"build failed: {r.stderr}"

    sim_src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sil_mcu_sim.c")
    wire_src = os.path.join(src_dir, "src", "common", "wire_frame.c")
    sim_bin = os.path.join(build_dir, "mcu-sim")
    r = subprocess.run(
        ["gcc", "-o", sim_bin, sim_src, wire_src,
         "-I", os.path.join(src_dir, "src", "common"),
         "-Wall", "-Wno-unused-parameter", "-Wno-missing-prototypes"],
        capture_output=True, text=True
    )
    assert r.returncode == 0, f"sim build failed: {r.stderr}"
    print("[BUILD] Both binaries compiled OK")

    gw_bin = os.path.join(build_dir, "sentinel-gw")

    # Start MCU sim (listens on 5000)
    mcu_proc = subprocess.Popen([sim_bin], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    time.sleep(0.3)

    # Start gateway
    gw_proc = subprocess.Popen([gw_bin], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    time.sleep(1.5)

    try:
        # === SIT-03: TCP ports ===
        print("\n--- SIT-03: TCP Channel Establishment ---")
        for port, name in [(TEL_PORT, "Telemetry:5001"), (DIAG_PORT, "Diagnostics:5002"), (CMD_PORT, "Command:5000")]:
            if wait_for_port(port):
                ok(f"{name} accepting connections")
            else:
                fail(name, "not accepting")

        # === SIT-10: Diagnostic interface ===
        print("\n--- SIT-10: Diagnostic Interface ---")
        try:
            ds = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ds.connect((LINUX_IP, DIAG_PORT))
            ds.settimeout(2)
            ok("Diagnostic connection established")
            ds.close()
        except Exception as e:
            fail("Diagnostic connect", str(e))

        # === SIT-05: Actuator command round-trip ===
        print("\n--- SIT-05: Actuator Command Round-Trip ---")
        try:
            cs = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            cs.connect((LINUX_IP, CMD_PORT))
            cs.settimeout(3)
            # Send actuator command
            send_frame(cs, MSG_ACTUATOR_CMD, b"\x08\x00\x15\x00\x00\x48\x42")
            time.sleep(0.5)
            msg_type, payload = recv_frame(cs, timeout=3)
            if msg_type == MSG_ACTUATOR_RSP:
                ok(f"Actuator response received: type=0x{msg_type:02X}, {len(payload)} bytes")
            else:
                fail("Actuator response", f"got type={msg_type}")
            cs.close()
        except Exception as e:
            fail("Actuator round-trip", str(e))

        # === Let MCU sim run for 3 seconds to accumulate messages ===
        time.sleep(3)

        # === Stability ===
        print("\n--- Stability ---")
        if gw_proc.poll() is None:
            ok("Gateway still running")
        else:
            fail("Gateway", f"exited {gw_proc.returncode}")

        if mcu_proc.poll() is None:
            ok("MCU sim still running")
        else:
            fail("MCU sim", f"exited {mcu_proc.returncode}")

    finally:
        # Stop
        for p in [gw_proc, mcu_proc]:
            if p.poll() is None:
                p.send_signal(signal.SIGTERM)
                try: p.wait(timeout=3)
                except: p.kill()

        # Verify MCU sim output
        print("\n--- MCU Sim Verification ---")
        mcu_out = mcu_proc.stdout.read().decode("utf-8", errors="replace")
        
        if "State" in mcu_out and "NORMAL" in mcu_out:
            ok("MCU reached NORMAL state")
        else:
            fail("MCU state", "never reached NORMAL")

        # Parse shutdown line
        for line in mcu_out.split("\n"):
            if "Shutting down" in line and "health" in line:
                parts = line.split()
                for i, p in enumerate(parts):
                    if "health" in p:
                        try:
                            hcount = int(parts[i-1])
                            if hcount >= 3:
                                ok(f"MCU sent {hcount} health messages")
                            else:
                                fail("MCU health", f"only {hcount}")
                        except: pass
                    if "sensor" in p and i > 0:
                        try:
                            scount = int(parts[i-1])
                            if scount >= 10:
                                ok(f"MCU sent {scount} sensor messages")
                            else:
                                fail("MCU sensor", f"only {scount}")
                        except: pass

        if "Received msg type 0x10" in mcu_out:
            ok("MCU received actuator command (0x10)")
        else:
            fail("MCU actuator rx", "never received")

        print(f"\n{'='*60}")
        total = passed + failed
        print(f"Results: {passed}/{total} passed, {failed} failed")
        print(f"{'='*60}")

    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
