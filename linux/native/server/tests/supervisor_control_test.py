"""Exercise local management commands against a real supervised process."""
import json
import os
import pathlib
import select
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix="4story-management-") as folder:
    fixture = pathlib.Path(folder) / "service"
    fixture.write_text(f"#!{sys.executable}\nimport signal,time\nsignal.signal(signal.SIGTERM,signal.SIG_IGN)\nprint('MANAGED_READY',flush=True)\nwhile True: time.sleep(1)\n")
    fixture.chmod(0o700)
    config = pathlib.Path(folder) / "service.conf"
    config.touch()
    process = subprocess.Popen([sys.executable, str(ROOT / "tools/server/supervise.py"),
        "--control-stdin", "--shutdown-seconds", "0.15", "--executable", str(fixture), str(config)],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
    pids = []

    def line():
        result = b""
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            assert select.select([process.stdout], [], [], max(0, deadline-time.monotonic()))[0], "Response timeout"
            byte = os.read(process.stdout.fileno(), 1)
            assert byte, "Supervisor exited"
            result += byte
            if byte == b"\n":
                if result.startswith(b"START pid="):
                    pids.append(int(result.split(b"=")[1]))
                return result
        raise AssertionError("Response timeout")

    def command(action, service=0):
        process.stdin.write((json.dumps({"action": action, "service": service})+"\n").encode())
        while True:
            response = line()
            if response.startswith(b"CONTROL "):
                return json.loads(response[8:])

    try:
        while line() != b"MANAGED_READY\n":
            pass
        assert pids
        assert command("status")["state"] == 4
        assert not command("start")["ok"]
        assert not command("stop", 1)["ok"]
        assert command("stop")["ok"]
        deadline = time.monotonic() + 5
        while command("status")["state"] != 1:
            assert time.monotonic() < deadline
            time.sleep(0.02)
        time.sleep(0.1)
        assert command("status")["state"] == 1
        assert command("start")["ok"]
        while line() != b"MANAGED_READY\n":
            pass
        assert len(pids) == 2
        assert pids[0] != pids[1] and command("status")["state"] == 4
        process.terminate()
        _, errors = process.communicate(timeout=5)
        assert process.returncode == 0, errors
        for pid in pids:
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                continue
            raise AssertionError("Managed service was not reaped")
    finally:
        if process.poll() is None:
            process.terminate()
            process.communicate(timeout=8)
print("PASS local supervisor start/stop/status, stopped-service persistence and reaping")
