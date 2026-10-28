#!/usr/bin/env python3
"""Exercise the real native checksum/cleanup bodies under ASan+UBSan and runner status."""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch, MagicMock

ROOT = Path(__file__).resolve().parents[4]
CLIENT = ROOT / 'source/4Story 5.0 Source Files/Client/TClient'
sys.path.insert(0, str(ROOT / 'native/client/app'))
spec = importlib.util.spec_from_file_location('client_run', ROOT / 'native/client/app/run.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class RuntimeRegression(unittest.TestCase):
    def test_original_native_bodies(self):
        sender = (CLIENT / 'TNetSender.cpp').read_text(encoding='latin-1')
        checksum = sender[sender.index('\tINT64 llChecksum;'):sender.index('\tvMSG.SetID(CS_LOGIN_REQ)')]
        macro = (CLIENT / 'MacroProtection.cpp').read_text(encoding='latin-1')
        update = macro[macro.index('void MacroProtection::Update()'):macro.index('void MacroProtection::NotifySkillUse')]
        # Only unrelated engine dependencies are replaced. The production method
        # declarations, cleanup body and checksum arithmetic are used unchanged.
        source = r'''
#include <cstdint>
#include <cstring>
#include <vector>
#include <map>
#include <cassert>
using INT64 = int64_t; using UINT64 = uint64_t; using DWORD = uint32_t;
using WORD = uint16_t; using BYTE = uint8_t;
struct Skill { WORD m_wSkillID; }; using LPTSKILL = Skill*;
class CTClientGame;
static DWORD now;
DWORD GetTickCount() { return now; }
#include "MacroProtection.h"
MacroProtection::MacroProtection(CTClientGame*) {}
MacroProtection::~MacroProtection() {}
INT64 checksum(WORD wVersion) {
''' + checksum + r'''
 return llChecksum;
}
''' + update + r'''
int main() {
 // Independent unsigned reference for every possible protocol version.
 for (unsigned version = 0; version < 65536; ++version) {
   uint64_t value = uint64_t(int64_t(version) * 2 - 500);
   const uint64_t body = value / 8, count = value % 8;
   for (uint64_t i = 0; i < count; ++i) value = (value ^ body) + UINT64_C(0x336c3aebf71a8b08);
   assert(uint64_t(checksum(WORD(version))) == value);
 }
 MacroProtection m(nullptr); Skill a{1}, b{2}, c{3};
 now = 1000;
 m.m_mapReusedHotkey[1] = {&a, 500, 2};  // exact expiry
 m.m_mapReusedHotkey[2] = {&b, 501, 3};  // retained
 m.m_mapReusedHotkey[3] = {&c, 0, 4};    // expired
 m.m_vUsedHotkey = {{&a, 0, 0}, {&b, 1, 0}};
 m.Update();
 assert(m.m_mapReusedHotkey.size() == 1 && m.m_mapReusedHotkey.at(2).m_bUsed == 3);
 assert(m.m_vUsedHotkey.size() == 2);
 m.Update(); assert(m.m_mapReusedHotkey.size() == 1);
 now = 30000; m.Update();
 assert(m.m_mapReusedHotkey.empty());
 assert(m.m_vUsedHotkey.size() == 1 && m.m_vUsedHotkey[0].pSKILL == &b);
 now = 30001; m.Update(); m.Update(); assert(m.m_vUsedHotkey.empty());
 // Consecutive erased nodes must not invalidate the next iteration.
 for (int i = 0; i < 1000; ++i) m.m_mapReusedHotkey[WORD(i)] = {&a, 0, 1};
 m.Update(); assert(m.m_mapReusedHotkey.empty());
}
'''
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            cpp, binary = directory / 'probe.cpp', directory / 'probe'
            cpp.write_text(source)
            subprocess.run(['clang++', '-std=c++14', '-g', '-O1', '-DFOURSTORY_CLIENT_NATIVE',
                            '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                            '-I', str(CLIENT), str(cpp), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

    def test_runner_status(self):
        for code, expected in [(0, 0), (1, 1), (-11, 139), (-6, 134)]:
            with self.subTest(code=code), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                binary = root / 'build/client/c4-plain/4story-client'
                binary.parent.mkdir(parents=True); binary.touch()
                process = MagicMock(); process.wait.return_value = code
                with patch.object(runner, 'ROOT', root), patch.object(sys, 'argv', ['run.py', '--plain', '--', '192.0.2.1', '4815', '1']), patch.object(runner.subprocess, 'Popen', return_value=process), patch.object(runner, 'select_binary', lambda binary: binary), contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(runner.main(), expected)
                self.assertEqual(next((root / 'runtime/client/runs').glob('*/status.txt')).read_text(), f'exit {code}\n')

    def test_requested_stop(self):
        for code, expected in [(-15, 0), (-6, 134), (-11, 139), (0, 0), (1, 1)]:
            with self.subTest(code=code), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                binary = root / 'build/client/c4-plain/4story-client'
                binary.parent.mkdir(parents=True); binary.touch()
                process = MagicMock(); process.wait.side_effect = [subprocess.TimeoutExpired('game', 1), code]
                with patch.object(runner, 'ROOT', root), patch.object(sys, 'argv', ['run.py', '--plain', '--seconds', '1', '--', '192.0.2.1']), patch.object(runner.subprocess, 'Popen', return_value=process), patch.object(runner, 'select_binary', lambda binary: binary), contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(runner.main(), expected)
                status = next((root / 'runtime/client/runs').glob('*/status.txt')).read_text()
                self.assertIn('stopped after 1 s (SIGTERM)' if code == -15 else f'exit {code} during requested stop', status)
                process.send_signal.assert_called_once()

    def test_forced_stop(self):
        # select_binary (build freshness, bundle signature) has its own tests;
        # these fixtures are empty files without a build record.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / 'build/client/c4-plain/4story-client'
            binary.parent.mkdir(parents=True); binary.touch()
            process = MagicMock()
            process.wait.side_effect = [subprocess.TimeoutExpired('game', 1), subprocess.TimeoutExpired('game', 10), -9]
            with patch.object(runner, 'ROOT', root), patch.object(sys, 'argv', ['run.py', '--plain', '--seconds', '1', '--', '192.0.2.1']), patch.object(runner.subprocess, 'Popen', return_value=process), patch.object(runner, 'select_binary', lambda binary: binary), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(runner.main(), 0)
            self.assertIn('stopped after 1 s (SIGKILL)', next((root / 'runtime/client/runs').glob('*/status.txt')).read_text())
            process.kill.assert_called_once()

    def test_default_address(self):
        with patch.object(runner.socket, 'socket') as socket:
            socket.return_value.__enter__.return_value.getsockname.return_value = ('192.0.2.7', 40000)
            self.assertEqual(runner.local_server_address(), '192.0.2.7')
            socket.return_value.__enter__.return_value.getsockname.return_value = ('127.0.0.1', 40000)
            with self.assertRaises(SystemExit): runner.local_server_address()
        with patch.object(runner.socket, 'socket', side_effect=OSError('no route')):
            with self.assertRaises(SystemExit): runner.local_server_address()

if __name__ == '__main__':
    unittest.main()
