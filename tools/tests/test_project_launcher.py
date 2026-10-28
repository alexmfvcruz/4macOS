#!/usr/bin/env python3
"""Launcher integration tests with disposable fake tools; no live cluster/GUI calls."""
import importlib.machinery
import importlib.util
import json
import os
import signal
import pty
import select
import termios
import time
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
LAUNCHER = ROOT / '4story.command'
loader = importlib.machinery.SourceFileLoader('fourstory_launcher', str(LAUNCHER))
spec = importlib.util.spec_from_loader(loader.name, loader)
module = importlib.util.module_from_spec(spec)
loader.exec_module(module)

FAKE = '''import json, os, sys
from pathlib import Path
out=Path(os.environ['TEST_OUTPUT'])
with out.open('a') as f: f.write(json.dumps({'tool':__file__, 'cwd':os.getcwd(), 'args':sys.argv[1:]})+'\\n')
if sys.argv[1:] == ['status']:
 print(os.environ.get('TEST_STATE','{}'))
elif os.environ.get('TEST_SIGNAL'):
 import signal
 os.kill(os.getpid(),int(os.environ['TEST_SIGNAL']))
else:
 sys.exit(int(os.environ.get('TEST_EXIT','0')))
'''


class LauncherTests(unittest.TestCase):
    def test_profiling_routes_without_launching_game(self):
        with patch.object(module, 'run', return_value=0) as run:
            self.assertEqual(module.main(['profile','doctor','test_client']),0)
            command = run.call_args.args[0]
            self.assertEqual(Path(command[1]).name,'profile.py')
            self.assertEqual(command[2:4],['doctor','test_client'])

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='4Story launcher ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve() / 'project with spaces'
        self.root.mkdir()
        self.output = Path(self.temp.name) / 'calls.jsonl'
        self.env = dict(os.environ, TEST_OUTPUT=str(self.output), PYTHONDONTWRITEBYTECODE='1')
        for key in ('TEST_STATE', 'TEST_SIGNAL', 'TEST_EXIT'):
            self.env.pop(key, None)
        for file in ('tools/server/native_cluster.py', 'native/client/app/run.py',
                     'native/client/app/build.py', 'tools/assets/render_asset.py'):
            p = self.root / file; p.parent.mkdir(parents=True, exist_ok=True); p.write_text(FAKE)
        build = self.root / 'native/happy/scripts/build-app.sh'
        build.parent.mkdir(parents=True, exist_ok=True); build.write_text('#!/bin/sh\nexit 0\n')
        for file in ('build/client/c4-plain/4story-client', 'build/client/c4-sanitized/4story-client'):
            p = self.root / file; p.parent.mkdir(parents=True, exist_ok=True); p.touch()
        (self.root / 'game').mkdir(parents=True)
        app = self.root / 'native/happy/.build/Happy Tool.app'; app.mkdir(parents=True)
        python = self.root / 'deps/python/assets/bin/python'; python.parent.mkdir(parents=True); python.symlink_to(sys.executable)
        for f in ('tools/assets/requirements.txt', 'deps/python/assets/.dependencies-installed'):
            (self.root / f).write_text('example pinned requirements\n')

    def call(self, *args, **kwargs):
        return subprocess.run([sys.executable, str(LAUNCHER), '--project', str(self.root), *args],
                              cwd=self.temp.name, env=self.env, capture_output=True, text=True, **kwargs)

    def calls(self):
        return [json.loads(s) for s in self.output.read_text().splitlines()] if self.output.exists() else []

    def test_help_menu_and_doctor_have_no_child_actions(self):
        for args in [('--help',), ('doctor',), ('setup',), ()]:
            r = self.call(*args, input='0\n')
            self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(self.calls())

    def test_client_defaults_and_argument_boundaries(self):
        r = self.call('client', '--server', '192.0.2.1', '--screen', '1280x800', '--seconds', '3')
        self.assertEqual(r.returncode, 0, r.stderr)
        row = self.calls()[0]
        self.assertEqual(row['cwd'], str(self.root))
        self.assertEqual(row['args'], ['--plain', '--game', str(self.root / 'game'),
                                      '--seconds', '3.0', '--screen', '1280x800', '--', '192.0.2.1', '4815', '1'])

    def test_checked_client_and_failures(self):
        self.env['TEST_EXIT'] = '7'
        r = self.call('client', '--checks', '--server', '192.0.2.1')
        self.assertEqual(r.returncode, 7, r.stderr)
        self.assertNotIn('--plain', self.calls()[0]['args'])
        self.env['TEST_SIGNAL'] = '15'
        r = self.call('client', '--server', '192.0.2.1')
        self.assertEqual(r.returncode, 143, r.stderr)

    def test_missing_game_does_not_run_or_setup(self):
        (self.root / 'build/client/c4-plain/4story-client').unlink()
        r = self.call('client', '--server', '192.0.2.1')
        self.assertEqual(r.returncode, 2)
        self.assertIn('build client', r.stderr)
        self.assertFalse(self.calls())

    def test_server_start_is_idempotent_for_ready_cluster(self):
        self.env['TEST_STATE'] = json.dumps({'supervisor_alive': True, 'phase': 'running',
            'roles': {r: {'ready': True} for r in ('TControlSvr', 'TLoginSvr', 'TWorldSvr', 'TMapSvr')}})
        r = self.call('server', 'start')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('already running', r.stdout)
        self.assertEqual([r['args'] for r in self.calls()], [['status']])

    def test_server_active_but_unready_is_not_started(self):
        self.env['TEST_STATE'] = '{"supervisor_alive": true, "phase": "starting"}'
        self.assertEqual(self.call('server', 'start').returncode, 2)
        self.assertEqual([r['args'] for r in self.calls()], [['status']])

    def test_server_start_and_stop_use_existing_controller(self):
        self.assertEqual(self.call('server', 'start').returncode, 0)
        self.assertEqual(self.call('server', 'stop').returncode, 0)
        self.assertEqual([r['args'] for r in self.calls()], [['status'], ['start'], ['status'], ['stop'], ['status']])

    def test_start_progress_tracks_readiness_and_preserves_failure(self):
        script = self.root / 'tools/server/native_cluster.py'
        script.write_text("""import json, os, sys, time
from pathlib import Path
if sys.argv[1] == 'status':
 print(json.dumps({'supervisor_alive':False,'supervisor_pid':123}))
else:
 p=Path('runtime/server-dev/state.json');p.parent.mkdir(parents=True,exist_ok=True)
 p.write_text(json.dumps({'supervisor_pid':456,'phase':'starting',
  'roles':{'TMapSvr':{'pid':999,'ready':False}}}))
 time.sleep(.4)
 failure=int(os.environ.get('TEST_EXIT','0'))
 roles={r:{'pid':999,'ready':not failure,'exit_code':1 if failure else None}
  for r in ('TControlSvr','TLoginSvr','TWorldSvr','TMapSvr')}
 p.write_text(json.dumps({'supervisor_pid':456,'phase':'failed' if failure else 'running',
  'roles':roles,'error':'Map readiness timeout' if failure else ''}))
 print('Controller diagnostics retained')
 sys.exit(failure)
""")
        result = self.call('server', 'start')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('4Story Cluster services', result.stdout)
        self.assertIn('Loading', result.stdout)
        self.assertIn('4/4 ready', result.stdout)
        self.assertIn('The 4Story Cluster is ready to play.', result.stdout)
        self.assertNotIn('Controller diagnostics retained', result.stdout)
        self.env['TEST_EXIT'] = '7'
        result = self.call('server', 'start')
        self.assertEqual(result.returncode, 7, result.stderr)
        self.assertIn('FAILED', result.stdout)
        self.assertIn('Map readiness timeout', result.stdout)
        self.assertIn('Startup failed.', result.stdout)
        self.assertIn('Controller diagnostics retained', result.stdout)

    def test_progress_interrupt_allows_controller_cleanup(self):
        marker = self.root / 'controller-ready'
        cleaned = self.root / 'controller-cleaned'
        (self.root / 'tools/server/native_cluster.py').write_text("""import json, signal, sys, time
from pathlib import Path
if sys.argv[1]=='status':
 print(json.dumps({'supervisor_alive':False}))
else:
 def stop(signum, frame):
  time.sleep(.2)
  Path('controller-cleaned').touch()
  sys.exit(130)
 signal.signal(signal.SIGINT,stop)
 Path('controller-ready').touch()
 while True: time.sleep(.05)
""")
        process = subprocess.Popen([sys.executable, str(LAUNCHER), '--project', str(self.root),
                                    'server', 'start'], cwd=self.temp.name, env=self.env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   text=True, start_new_session=True)
        try:
            deadline = time.monotonic() + 5
            while not marker.exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(.02)
            self.assertTrue(marker.exists())
            os.killpg(process.pid, signal.SIGINT)
            process.communicate(timeout=5)
            self.assertTrue(cleaned.exists())
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()

    def test_compact_status_and_raw_json(self):
        self.env['TEST_STATE'] = json.dumps({'supervisor_alive':False,'phase':'stopped',
            'roles':{'TMapSvr':{'pid':123,'port':5815,'ready':True,'exit_code':0}}})
        result = self.call('server', 'status')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('4Story Cluster services', result.stdout)
        self.assertIn('Off', result.stdout)
        self.assertNotIn('Last recorded PID', result.stdout)
        self.assertNotIn('5815', result.stdout)
        self.assertNotIn('Running', result.stdout)
        result = self.call('server', 'status', '--json')
        self.assertEqual(json.loads(result.stdout)['phase'], 'stopped')

    def test_status_expansion_and_live_uptime(self):
        state = {'supervisor_alive':True,'phase':'running',
                 'roles':{'TControlSvr':{'pid':123,'ready':True}}}
        with patch.object(module, 'service_uptime', side_effect=['01:20',None,None,None]):
            text = '\n'.join(module.status_lines(state, {1}, color=True))
        self.assertIn('01:20', text)
        self.assertIn('Running', text)
        self.assertIn('\033[32m', text)
        self.assertIn('Last recorded PID: 123', text)
        self.assertNotIn('TLoginSvr.log', text)
        screen = unittest.mock.MagicMock()
        screen.getmaxyx.return_value = (32, 100)
        screen.getch.side_effect = [module.curses.KEY_MOUSE, ord('R'), module.curses.KEY_MOUSE, KeyboardInterrupt()]
        with patch.object(module, 'cluster_status', return_value=state) as status, \
             patch.object(module, 'service_uptime', return_value=None), \
             patch.object(module.curses, 'curs_set', return_value=1), \
             patch.object(module.curses, 'mousemask') as mousemask, \
             patch.object(module.curses, 'mouseinterval'), \
             patch.object(module.curses, 'raw'), \
             patch.object(module.curses, 'noraw'), \
             patch.object(module.curses, 'has_colors', return_value=False), \
             patch.object(module.curses, 'getmouse', return_value=(0, 6, 3, 0, module.curses.BUTTON1_PRESSED)):
            self.assertEqual(module.status_dashboard(screen, self.root), 0)
        self.assertEqual(status.call_count, 2)
        self.assertEqual(screen.erase.call_count, 4)
        self.assertEqual(screen.refresh.call_count, 4)
        mousemask.assert_called_with(0)
        details = [call.args[2] for call in screen.addnstr.call_args_list if 'Last recorded PID' in call.args[2]]
        self.assertEqual(len(details), 2)

    def test_dashboard_mouse_refresh_and_terminal_cleanup_in_pty(self):
        master, slave = pty.openpty()
        original = termios.tcgetattr(slave)
        env = dict(self.env, TERM='xterm-256color')
        process = subprocess.Popen([sys.executable, str(LAUNCHER), '--project', str(self.root),
                                    'server', 'status'], stdin=slave, stdout=slave, stderr=slave,
                                   cwd=self.temp.name, env=env, start_new_session=True)
        def read_until(expected):
            data = b''
            deadline = time.monotonic() + 5
            while expected not in data and time.monotonic() < deadline:
                if select.select([master], [], [], .1)[0]:
                    data += os.read(master, 65536)
            self.assertIn(expected, data)
            return data
        try:
            initial = read_until(b'Click a service')
            mouse = b'\x1b[<0;5;4M' if b'1006h' in initial else b'\x1b[M' + bytes((32, 37, 36))
            os.write(master, mouse)
            read_until(b'Last recorded PID')
            os.write(master, b'R')
            deadline = time.monotonic()+5
            while len(self.calls()) < 2 and time.monotonic() < deadline:
                time.sleep(.02)
            self.assertEqual(len(self.calls()), 2)
            os.write(master, b'\x03')
            read_until(b'\x1b[?1049l')
            process.wait(timeout=5)
            self.assertEqual(process.returncode, 0)
            restored = termios.tcgetattr(slave)
            # macOS sets PENDIN when changing modes to retype pending input.
            # It is transient kernel bookkeeping, not an input/echo mode.
            restored[3] &= ~getattr(termios, 'PENDIN', 0)
            original[3] &= ~getattr(termios, 'PENDIN', 0)
            self.assertEqual(restored, original)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            os.close(master)
            os.close(slave)

    def test_uptime_rejects_reused_pid(self):
        with patch.object(module.subprocess, 'run') as query:
            query.return_value.returncode = 0
            query.return_value.stdout = '01:20 /usr/bin/something-else\n'
            self.assertIsNone(module.service_uptime('TMapSvr', {'pid':123}, True))
            query.return_value.stdout = '01:20 /project/TMapSvr\n'
            self.assertEqual(module.service_uptime('TMapSvr', {'pid':123}, True), '01:20')
            query.reset_mock()
            self.assertIsNone(module.service_uptime('TMapSvr', {'pid':123}, False))
            query.assert_not_called()

    def test_controller_status_failure_keeps_diagnostics_and_code(self):
        (self.root / 'tools/server/native_cluster.py').write_text(
            "import sys; print('Cannot read state file', file=sys.stderr); sys.exit(9)\n")
        for options in [('server','status'), ('server','status','--json')]:
            result = self.call(*options)
            self.assertEqual(result.returncode, 9)
            self.assertIn('Cannot read state file', result.stderr)

    def test_stop_reports_outcome_in_plain_language(self):
        result = self.call('server', 'stop')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('Stopping 4Story Cluster services', result.stdout)
        self.assertIn('4Story Cluster is off', result.stdout)
        self.assertNotIn('exit_code=', result.stdout)
        self.env['TEST_EXIT']='7'
        result = self.call('server', 'stop')
        self.assertEqual(result.returncode, 7, result.stderr)
        self.assertIn('Shutdown failed or is incomplete', result.stdout)
        state={'roles':{'TMapSvr':{'pid':123,'exit_code':1}}}
        self.assertIn('Stopped after an error', '\n'.join(module.shutdown_lines(state, 3)))

    def test_every_dry_run_has_no_child_actions(self):
        for args in [('server', 'start'), ('server', 'stop'), ('server', 'status'),
                     ('client',), ('client', '--checks'), ('happy',),
                     ('assets', 'show', 'monster:31036'), ('build', 'client'), ('build', 'happy')]:
            with self.subTest(args=args):
                r = self.call('--dry-run', *args)
                self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(self.calls())

    def test_asset_environment_is_reused_without_wrapper(self):
        r = self.call('assets', 'show', 'monster:31036', '--width', '60')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(self.calls()[0]['args'], ['show', 'monster:31036', '--width', '60'])
        self.assertTrue(self.calls()[0]['tool'].endswith('tools/assets/render_asset.py'))
        (self.root / 'deps/python/assets/.dependencies-installed').write_text('stale\n')
        self.assertEqual(self.call('assets', 'list').returncode, 2)
        self.assertEqual(len(self.calls()), 1)

    def test_latest_log_uses_actual_update_time(self):
        for name, when in [('20991231-000000', 1), ('20000101-000000', 2)]:
            log = self.root / 'runtime/client/runs' / name / 'log.txt'
            log.parent.mkdir(parents=True); log.write_text('private log not printed\n')
            os.utime(log, ns=(when, when))
        r = self.call('logs')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('20000101-000000', r.stdout)
        self.assertNotIn('private log', r.stdout)
        self.assertFalse(self.calls())

    def test_happy_open_uses_one_space_containing_argument(self):
        with patch.object(module.subprocess, 'run') as run:
            run.return_value.returncode = 0
            self.assertEqual(module.main(['--project', str(self.root), 'happy']), 0)
            self.assertEqual(run.call_args.args[0], ['/usr/bin/open', str(self.root / 'native/happy/.build/Happy Tool.app')])

    def test_server_does_not_expose_database_reset(self):
        r = self.call('server', 'reset-db')
        self.assertEqual(r.returncode, 2)
        self.assertFalse(self.calls())

    def test_bad_client_values_do_not_start_game(self):
        for option, value in [('--seconds', '-1'), ('--seconds', 'nan'), ('--port', '0'), ('--capture', '0'), ('--screen', 'oops'), ('--screen', '0x800')]:
            self.assertEqual(self.call('client', option, value).returncode, 2)
        self.assertFalse(self.calls())

    def test_default_address_failure_and_loopback(self):
        with patch.object(module.socket, 'socket') as socket:
            socket.return_value.__enter__.return_value.getsockname.return_value = ('192.0.2.7', 40000)
            self.assertEqual(module.local_address(), '192.0.2.7')
            socket.return_value.__enter__.return_value.getsockname.return_value = ('127.0.0.1', 40000)
            with self.assertRaises(module.LaunchError): module.local_address()
        with patch.object(module.socket, 'socket', side_effect=OSError()):
            with self.assertRaises(module.LaunchError): module.local_address()


if __name__ == '__main__':
    unittest.main()
