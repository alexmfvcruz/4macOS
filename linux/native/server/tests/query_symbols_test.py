"""Check actual combined-binary symbols for overlapping legacy query names."""
import pathlib
import subprocess
ROOT = pathlib.Path(__file__).resolve().parents[3]
binary = ROOT / "native/server/build/combined_roles_test"
symbols = subprocess.check_output(["nm", "-C", str(binary)], text=True).splitlines()
expected = {
    "CTBLGroup": {"TControlSvr", "TLoginSvr"},
    "CSPLoadService": {"TControlSvr", "TLoginSvr", "TWorldSvr", "TMapSvr"},
}
for name, roles in expected.items():
    found = {line.split("queries::", 1)[1].split("::", 1)[0]
             for line in symbols if "queries::" in line and f"::{name}::" in line}
    assert found == roles, (name, found)
    assert not any(f" {name}::" in line for line in symbols), f"Global {name} symbol"
print("PASS combined-binary query classes have distinct role namespaces")
