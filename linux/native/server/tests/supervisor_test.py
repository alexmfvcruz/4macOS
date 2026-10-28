"""Force a real process crash and uncooperative shutdown, then check reaping."""
import os
import pathlib
import select
import signal
import subprocess
import sys
import tempfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix="4story-supervisor-") as folder:
    fixture = pathlib.Path(folder) / "stuck-service"
    fixture.write_text(f"#!{sys.executable}\nimport signal,time\nsignal.signal(signal.SIGTERM,signal.SIG_IGN)\nprint('CHILD_RUNNING',flush=True)\nwhile True: time.sleep(1)\n")
    fixture.chmod(0o700)
    config = pathlib.Path(folder) / "fixture.conf"
    config.touch()
    supervisor = subprocess.Popen([sys.executable, str(ROOT / "tools/server/supervise.py"),
        "--executable", str(fixture), "--shutdown-seconds", "0.15", "--retry-seconds", "0.03", str(config)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
    pids = []

    def started():
        deadline = time.monotonic() + 5
        pid = None
        running = False
        line = b""
        while time.monotonic() < deadline:
            if not select.select([supervisor.stdout], [], [], max(0, deadline-time.monotonic()))[0]:
                break
            byte = os.read(supervisor.stdout.fileno(), 1)
            assert byte, "Supervisor exited before starting child"
            line += byte
            if byte != b"\n":
                continue
            if line.startswith(b"START pid="):
                pid = int(line.split(b"=")[1]);pids.append(pid)
            if line.startswith(b"CHILD_RUNNING"):
                running = True
            line = b""
            if pid and running:
                return pid
        raise AssertionError("Child did not start")

    try:
        first = started();os.kill(first, signal.SIGKILL)
        second = started();assert first != second
        supervisor.terminate()
        output, errors = supervisor.communicate(timeout=5)
        assert supervisor.returncode == 0, errors
        assert f"FORCE_STOP pid={second}".encode() in output and b"SUPERVISOR_STOPPED" in output
        for pid in pids:
            try:
                os.kill(pid, 0)
            except ProcessLookupError:
                continue
            raise AssertionError("Supervised child still exists")
    finally:
        if supervisor.poll() is None:
            supervisor.kill()
        supervisor.wait()
        for pid in pids:
            try:
                os.killpg(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
print("PASS supervisor crash restart, hard shutdown deadline and child reaping")
