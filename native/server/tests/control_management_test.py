"""Connect compiled original Control fragments to the live supervisor backend."""
import os
import pathlib
import select
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix="4story-control-") as folder:
    fixture=pathlib.Path(folder)/"service"
    fixture.write_text(f"#!{sys.executable}\nimport time\nprint('READY',flush=True)\nwhile True: time.sleep(1)\n")
    fixture.chmod(0o700)
    config=pathlib.Path(folder)/"service.conf"
    config.touch()
    supervisor=subprocess.Popen([sys.executable,str(ROOT/"tools/server/supervise.py"),
        "--control-stdin","--executable",str(fixture),str(config)],
        stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0)
    try:
        line=b""
        while line!=b"READY\n":
            line=b""
            while not line.endswith(b"\n"):
                assert select.select([supervisor.stdout],[],[],5)[0],"Startup timeout"
                byte=os.read(supervisor.stdout.fileno(),1)
                assert byte,"Supervisor exited"
                line+=byte
        subprocess.run([str(ROOT/"build/server/control_management_test"),
            str(supervisor.stdin.fileno()),str(supervisor.stdout.fileno())],
            pass_fds=(supervisor.stdin.fileno(),supervisor.stdout.fileno()),check=True,timeout=10)
    finally:
        supervisor.terminate()
        _,errors=supervisor.communicate(timeout=8)
        assert supervisor.returncode==0,errors
