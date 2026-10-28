#!/usr/bin/env python3
"""Exercise real C++/Rust transport against this checkout's isolated SQL service.
Never points to TGLOBAL_GSP. No account/password strings are printed.
"""
import http.server
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import threading
import time
import urllib.request
ROOT=Path(__file__).resolve().parents[3]
record=json.loads((ROOT/'runtime/accounts/service.json').read_text())
assert record['database'].startswith('FourStory_Accounts_Test_')
class Proxy(http.server.BaseHTTPRequestHandler):
    count=0
    def do_CONNECT(self):Proxy.count+=1;self.send_error(502)
    def do_POST(self):Proxy.count+=1;self.send_error(502)
    def log_message(self,*args):pass
proxy=http.server.ThreadingHTTPServer(('127.0.0.1',0),Proxy)
threading.Thread(target=proxy.serve_forever,daemon=True).start()
env=dict(os.environ,FOURSTORY_ACCOUNTS_ABI_HTTP='1',ASAN_OPTIONS='detect_leaks=0',
         HTTP_PROXY='http://127.0.0.1:'+str(proxy.server_port),HTTPS_PROXY='http://127.0.0.1:'+str(proxy.server_port))
for key in ['NO_PROXY','no_proxy']:env.pop(key,None)
exe=ROOT/'build/accounts/core-ffi-test'
url='https://127.0.0.1:'+str(record['port'])
stamp=str(time.time_ns())
try:
    env.update(FOURSTORY_ACCOUNTS_URL=url+'/v1/accounts',FOURSTORY_ACCOUNTS_ABI_NAME='Tls'+stamp,FOURSTORY_ACCOUNTS_CA=str(ROOT/'runtime/accounts/tls/ca.pem'))
    subprocess.run([str(exe)],env=env,check=True)
    reject=dict(env,FOURSTORY_ACCOUNTS_ABI_TLS_REJECT='1',FOURSTORY_ACCOUNTS_ABI_NAME='Reject'+stamp)
    reject.pop('FOURSTORY_ACCOUNTS_CA',None)
    subprocess.run([str(exe)],env=reject,check=True)
    with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
    child=subprocess.Popen([str(ROOT/'build/accounts/release/fourstory-account-service'),record['config'],record['database'],'loopback-http','127.0.0.1:'+str(port)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        time.sleep(.3);assert child.poll() is None
        env.update(FOURSTORY_ACCOUNTS_URL='http://127.0.0.1:'+str(port)+'/v1/accounts',FOURSTORY_ACCOUNTS_DEV_HTTP='1',FOURSTORY_ACCOUNTS_ABI_NAME='Http'+stamp)
        subprocess.run([str(exe)],env=env,check=True)
    finally:child.terminate();child.wait(timeout=5)
    assert Proxy.count==0
    print('PASS HTTPS verification and no inherited proxy use, including explicit loopback HTTP')
    # A partial body must not hold an admission permit indefinitely.
    context=ssl.create_default_context(cafile=str(ROOT/'runtime/accounts/tls/ca.pem'))
    raw=context.wrap_socket(socket.create_connection(('127.0.0.1',record['port'])),server_hostname='127.0.0.1');raw.settimeout(7)
    try:
        raw.sendall(b'POST /v1/accounts HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: 1000\r\n\r\n{')
        start=time.monotonic();response=raw.recv(4096)
        assert b'400 Bad Request' in response and 4.5<time.monotonic()-start<7
    finally:raw.close()
    print('PASS unfinished request body rejected within five seconds')
finally:proxy.shutdown()
