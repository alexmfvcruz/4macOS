#!/usr/bin/env python3
"""Exercise real helper/IPC code with a compile-time fake HID backend: no OS writes."""
import os,select,signal,subprocess,tempfile,time,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
BINARY=ROOT/'runtime/legacy/mac-keyboard-poc/keyboard-probe-fake'
class ProbeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BINARY.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run(['clang++','-std=c++17','-fobjc-arc','-Wall','-Wextra','-Werror',
            '-DFOURSTORY_KEYBOARD_PROBE_TEST',str(Path(__file__).with_name('probe.mm')),
            '-framework','Carbon','-framework','ApplicationServices','-framework','AppKit','-framework','CoreFoundation','-framework','IOKit','-o',str(BINARY)],check=True)
    def setUp(self):
        self.folder=tempfile.TemporaryDirectory(dir=BINARY.parent)
        self.addCleanup(self.folder.cleanup)
        self.path=Path(self.folder.name);self.state=self.path/'mode';self.state.write_text('0\n')
        self.children=[];self.logs=[]
        self.addCleanup(self.cleanup_processes)
    def cleanup_processes(self):
        for p in self.children:
            if p.poll() is None:
                p.stdin.close()
                try:p.wait(timeout=10)
                except subprocess.TimeoutExpired:p.kill();p.wait()
            if p.stdout:p.stdout.close()
            if p.stdin and not p.stdin.closed:p.stdin.close()
        for f in self.logs:f.close()
    def start(self,**extra):
        log=(self.path/('log-'+str(len(self.children)))).open('w+');self.logs.append(log)
        env=dict(os.environ,FOURSTORY_KEYBOARD_FAKE_STATE=str(self.state),**extra)
        p=subprocess.Popen([str(BINARY),str(self.path/'lock'),'--script-test'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=log,env=env,text=True,bufsize=1)
        self.children.append(p);return p
    def line(self,p,timeout=8):
        self.assertTrue(select.select([p.stdout],[],[],timeout)[0],'timed out waiting for probe result')
        return p.stdout.readline().strip()
    def command(self,p,c):p.stdin.write(c+'\n');p.stdin.flush();return self.line(p)
    def wait_mode(self,mode):
        end=time.monotonic()+10
        while time.monotonic()<end:
            if self.state.read_text().strip()==str(mode):return
            time.sleep(.02)
        self.fail('helper did not restore fake mode')
    def test_round_trip_original_zero_and_one(self):
        for original in (0,1):
            self.state.write_text(str(original)+'\n');p=self.start();self.assertEqual(self.line(p),'READY '+str(original))
            self.assertEqual(self.command(p,'E'),'REPLY 0 %d 1'%original)
            self.assertEqual(self.command(p,'R'),'REPLY 0 %d %d'%(original,original))
            self.assertEqual(self.command(p,'Q'),'FINISH 0');self.assertEqual(p.wait(timeout=5),0)
            self.assertEqual(self.state.read_text().strip(),str(original))
    def test_external_change_before_enable_is_preserved(self):
        p=self.start();self.assertEqual(self.line(p),'READY 0');self.state.write_text('1\n')
        self.assertNotEqual(self.command(p,'E').split()[1],'0');self.assertEqual(self.command(p,'Q'),'FINISH 0')
        self.assertEqual(self.state.read_text().strip(),'1')
    def test_external_change_during_lease_is_preserved(self):
        self.state.write_text('1\n');p=self.start();self.line(p);self.command(p,'E');self.state.write_text('0\n')
        self.assertNotEqual(self.command(p,'R').split()[1],'0');self.assertNotEqual(self.command(p,'Q'),'FINISH 0')
        self.assertEqual(self.state.read_text().strip(),'0')
    def test_parent_sigkill_recovers(self):
        p=self.start();self.line(p);self.command(p,'E');self.assertEqual(self.state.read_text().strip(),'1')
        p.kill();p.wait(timeout=5);self.wait_mode(0)
    def test_late_enable_after_timeout_recovers(self):
        p=self.start(FOURSTORY_KEYBOARD_FAKE_ENABLE_DELAY_MS='4200');self.line(p)
        self.assertNotEqual(self.command(p,'E').split()[1],'0')
        self.assertEqual(self.command(p,'Q'),'FINISH 0');self.assertEqual(self.state.read_text().strip(),'0')
    def test_restore_failure_is_nonzero(self):
        p=self.start(FOURSTORY_KEYBOARD_FAKE_FAIL_RESTORE='1');self.line(p);self.command(p,'E')
        self.assertNotEqual(self.command(p,'R').split()[1],'0')
        self.assertNotEqual(self.command(p,'F'),'FINISH 0')
        self.assertNotEqual(self.command(p,'F'),'FINISH 0')
        self.assertNotEqual(self.command(p,'Q'),'FINISH 0')
        self.assertNotEqual(p.wait(timeout=5),0)
    def test_second_instance_cannot_acquire_lease(self):
        p=self.start();self.line(p);self.command(p,'E');other=self.start()
        self.assertEqual(other.wait(timeout=5),1)
        self.assertEqual(self.state.read_text().strip(),'1');self.assertEqual(self.command(p,'Q'),'FINISH 0')
        self.assertEqual(self.state.read_text().strip(),'0')
if __name__=='__main__':unittest.main(verbosity=2)
