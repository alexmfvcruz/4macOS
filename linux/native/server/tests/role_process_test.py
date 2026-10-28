"""Verify each role in the actual executable, without claiming DB/game readiness."""
import pathlib
import select
import socket
import struct
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
EXE = ROOT / "native/server/build/fourstory-service"
with tempfile.TemporaryDirectory(prefix="4story-roles-") as folder:
    for role in ("control", "login", "world", "map"):
        with socket.socket() as dependency, socket.socket() as log_receiver:
            dependency.bind(("127.0.0.1", 0)); dependency.listen()
            log_receiver.bind(("127.0.0.1", 0)); log_receiver.listen()
            with socket.socket() as reservation:
                reservation.bind(("127.0.0.1", 0))
                port = reservation.getsockname()[1]
            settings = (f"service={role}\nport={port}\ndb_dsn=unused\ndb_user=unused\n"
                        "db_password_env=UNUSED\n")
            if role != "control":
                settings += "server_id=1\n"
            if role in ("world", "map"):
                settings += "group_id=1\n"
            if role == "map":
                settings += (f"world_address=127.0.0.1\nworld_port={dependency.getsockname()[1]}\n"
                             f"log_address=127.0.0.1\nlog_port={log_receiver.getsockname()[1]}\n")
            config = pathlib.Path(folder) / f"{role}.conf"
            config.write_text(settings)
            child = subprocess.Popen([str(EXE), str(config)], stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, text=True)
            try:
                assert select.select([child.stdout], [], [], 5)[0], f"No {role} listener announcement"
                assert child.stdout.readline().strip() == f"INFRASTRUCTURE_LISTENING port={port}"
                with socket.create_connection(("127.0.0.1", port), timeout=2) as peer:
                    peer.sendall(struct.pack("<HHIQ", 16, 0xffff, 0, 0))
                    assert peer.recv(1) == b"", f"{role} dispatch did not reject unimplemented packet"
                if role == "map":
                    dependency.settimeout(2)
                    upstream, _ = dependency.accept()
                    upstream.close()
                    # The original Map policy stops on World connection loss.
                    output, errors = child.communicate(timeout=5)
                    assert child.returncode != 0 and "Required service connection lost" in errors
                else:
                    child.terminate()
                    output, errors = child.communicate(timeout=5)
                    assert child.returncode == 0 and "STOPPED" in output, errors
            finally:
                if child.poll() is None:
                    child.kill()
                child.wait()
print("PASS executable Control/Login/World/Map dispatch, signal shutdown and Map dependency-loss failure")
