#!/usr/bin/env python3
"""Manage this checkout's local registration service. Never resets game servers.
prepare adds a unique name index, profile table and create-only procedure, then
creates a dedicated SQL login with EXECUTE on that procedure only. Existing
account passwords and game data are preserved. No migration runs on start.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import secrets
import signal
import subprocess
import sys
import time
import tempfile
import fcntl

ROOT=Path(__file__).resolve().parents[2]
MAIN=ROOT.parent.parent if ROOT.name=='codex' and ROOT.parent.name=='.local' else ROOT
LOCAL=ROOT/'runtime/accounts'
BINARY=ROOT/'build/accounts/release/fourstory-account-service'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('action',choices=['prepare','start','status','stop'])
p.add_argument('--database',default='TGLOBAL_GSP',help='Game database, or a newly created FourStory_Accounts_Test_ fixture')
p.add_argument('--port',type=int,default=4820)
a=p.parse_args()
if a.database!='TGLOBAL_GSP' and not (a.database.startswith('FourStory_Accounts_Test_') and a.database.replace('_','').isalnum()):
    p.error('Only the game copy or isolated account fixtures may be changed')
if not 1024<=a.port<=65535:p.error('Use a port from 1024 to 65535')
LOCAL.mkdir(parents=True,exist_ok=True)
config=LOCAL/(a.database+'.env')
record=LOCAL/'service.json'

def private(path,content):
    fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'w') as f:f.write(content)

def alive(info):
    try:
        actual=subprocess.run(['ps','-p',str(info['pid']),'-o','command='],capture_output=True,text=True,check=True).stdout
        return str(BINARY) in actual and str(info['config']) in actual
    except (subprocess.CalledProcessError,KeyError):return False

try:
    if a.action=='prepare':
        venv=MAIN/'deps/python/database/bin/python'
        try:import pymssql
        except ImportError:
            if not venv.is_file():raise RuntimeError('Existing SQL Python environment missing; nothing installed')
            raise SystemExit(subprocess.run([str(venv),str(Path(__file__).resolve()),*sys.argv[1:]],cwd=ROOT).returncode)
        prepare_lock=(LOCAL/'prepare.lock').open('a')
        fcntl.flock(prepare_lock,fcntl.LOCK_EX)
        sys.path.insert(0,str(MAIN/'tools/database'))
        import native_cluster_db as db
        if not BINARY.is_file():raise RuntimeError('Build native/accounts first')
        subprocess.run([str(BINARY),str(MAIN/'runtime/server-dev/sql-server.env'),a.database,'migrate'],check=True)
        user='FourStoryRegistration_'+hashlib.sha256(a.database.encode()).hexdigest()[:12]
        existing=config.exists()
        secret=dict(line.split('=',1) for line in config.read_text().splitlines())['MSSQL_SA_PASSWORD'] if existing else secrets.token_urlsafe(36)
        connection=db.connect()
        try:
            # New SQL identity only. Never reset an existing SQL or game password.
            exists=db.query(connection,'SELECT COUNT(*) n FROM sys.sql_logins WHERE name=%s',(user,))[0]['n']
            if exists and not existing:raise RuntimeError('SQL login already exists without this private config; manual review required')
            if not exists:
                db.query(connection,'CREATE LOGIN '+db.ident(user)+' WITH PASSWORD='+db.literal(secret)+', CHECK_POLICY=ON')
            db.query(connection,'USE '+db.ident(a.database))
            if not db.query(connection,'SELECT name FROM sys.database_principals WHERE name=%s',(user,)):
                db.query(connection,'CREATE USER '+db.ident(user)+' FOR LOGIN '+db.ident(user))
            db.query(connection,'GRANT EXECUTE ON dbo.NativeRegisterAccount TO '+db.ident(user))
            if not existing:private(config,'MSSQL_USER='+user+'\nMSSQL_SA_PASSWORD='+secret+'\n')
        finally:connection.close()
        tls=LOCAL/'tls'
        if not (tls/'server.pem').exists():
            published_tls=tls
            tls=Path(tempfile.mkdtemp(prefix='tls-build-',dir=LOCAL))
            # Existing OpenSSL only; trust stays in this checkout, never the system.
            private(tls/'ca.cnf','[req]\ndistinguished_name=dn\nx509_extensions=ca\nprompt=no\n[dn]\nCN=4Story local registration CA\n[ca]\nbasicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign,cRLSign\nsubjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid:always\n')
            private(tls/'server.cnf','[req]\ndistinguished_name=dn\nreq_extensions=ext\nprompt=no\n[dn]\nCN=localhost\n[ext]\nsubjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1\n[server]\nsubjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1\nbasicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid,issuer\n')
            commands=[['req','-x509','-newkey','rsa:2048','-nodes','-days','3650','-config',str(tls/'ca.cnf'),'-keyout',str(tls/'ca.key'),'-out',str(tls/'ca.pem')],
                      ['req','-newkey','rsa:2048','-nodes','-config',str(tls/'server.cnf'),'-keyout',str(tls/'server.key'),'-out',str(tls/'server.csr')],
                      ['x509','-req','-in',str(tls/'server.csr'),'-CA',str(tls/'ca.pem'),'-CAkey',str(tls/'ca.key'),'-CAcreateserial','-days','365','-extfile',str(tls/'server.cnf'),'-extensions','server','-out',str(tls/'server.pem')]]
            old=os.umask(0o077)
            try:
                for command in commands:subprocess.run(['/usr/bin/openssl',*command],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
            finally:os.umask(old)
            if published_tls.exists():published_tls.rename(LOCAL/('tls-incomplete-'+str(time.time_ns())))
            tls.rename(published_tls)
        print('Prepared create-only registration and checkout-local TLS. Existing passwords unchanged.')
    elif a.action=='start':
        if record.exists() and alive(json.loads(record.read_text())):raise RuntimeError('This checkout already has an account service; use status')
        tls=LOCAL/'tls'
        if not config.is_file() or not (tls/'server.pem').is_file():raise RuntimeError('Run prepare first; start never migrates')
        with (LOCAL/'service.log').open('ab') as log:
            proc=subprocess.Popen([str(BINARY),str(config),a.database,'serve','127.0.0.1:'+str(a.port),str(tls/'server.pem'),str(tls/'server.key')],stdout=log,stderr=log,start_new_session=True)
        time.sleep(.3)
        if proc.poll() is not None:raise RuntimeError('Service failed; inspect runtime/accounts/service.log')
        record.write_text(json.dumps(dict(pid=proc.pid,config=str(config),database=a.database,port=a.port)))
        print('Registration HTTPS started on this Mac, port '+str(a.port))
    elif a.action=='status':
        info=json.loads(record.read_text()) if record.exists() else {}
        print('Registration service '+('running on port '+str(info['port']) if info and alive(info) else 'stopped'))
    else:
        info=json.loads(record.read_text()) if record.exists() else {}
        if info and alive(info):os.kill(info['pid'],signal.SIGTERM);print('Registration service stop requested')
        else:print('Registration service already stopped')
except (OSError,RuntimeError,ValueError,subprocess.CalledProcessError):
    raise SystemExit('Account service action failed. Check private configuration and schema; credentials are not logged.')
