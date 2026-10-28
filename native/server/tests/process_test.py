"""Observe the actual native executable's listener and signal shutdown."""
import pathlib
import os
import signal
import select
import socket
import subprocess
import struct
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
EXE = ROOT / "build/server/fourstory-service"
with tempfile.TemporaryDirectory(prefix="4story-process-") as folder:
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    config = pathlib.Path(folder) / "control.conf"
    config.write_text(f"service=control\nport={port}\ndb_dsn=unused\ndb_user=unused\ndb_password_env=UNUSED\n")
    child = subprocess.Popen([str(EXE), str(config)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        assert select.select([child.stdout], [], [], 5)[0], "No listener announcement"
        assert child.stdout.readline().strip() == f"INFRASTRUCTURE_LISTENING port={port}"
        os.kill(child.pid, signal.SIGPIPE)
        with socket.create_connection(("127.0.0.1", port), timeout=2) as peer:
            # A complete original header reaches Control dispatch. With no game
            # handler installed the connection must close, rather than hang.
            peer.sendall(struct.pack("<HHIQ", 16, 0xffff, 0, 0))
            assert peer.recv(1) == b"", "Unimplemented packet did not close"
        child.terminate()
        output, errors = child.communicate(timeout=5)
        assert child.returncode == 0, errors
        assert "STOPPED" in output
    finally:
        if child.poll() is None:
            child.kill()
        child.wait()
    config.write_text("service=world\nport=invalid\n")
    rejected = subprocess.run([str(EXE), str(config)], capture_output=True, text=True, timeout=5)
    assert rejected.returncode != 0 and "INFRASTRUCTURE_LISTENING" not in rejected.stdout
print("PASS native service process, observable TCP listener, SIGTERM shutdown and invalid-config rejection")
