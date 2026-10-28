#!/usr/bin/env python3
"""Restricted-login and SQL lock deadline checks; isolated fixtures only."""
import json
from pathlib import Path
import ssl
import sys
import time
import urllib.request
import urllib.error
ROOT=Path(__file__).resolve().parents[3]
MAIN=ROOT.parent.parent if ROOT.name=='codex' and ROOT.parent.name=='.local' else ROOT
sys.path.insert(0,str(MAIN/'tools/database'))
import native_cluster_db as db
import pymssql
info=json.loads((ROOT/'runtime/accounts/service.json').read_text())
assert info['database'].startswith('FourStory_Accounts_Test_')
config=dict(l.split('=',1) for l in Path(info['config']).read_text().splitlines())
c=pymssql.connect(server='127.0.0.1',port=14330,user=config['MSSQL_USER'],password=config['MSSQL_SA_PASSWORD'],database=info['database'],autocommit=True)
try:
    row=db.query(c,"SELECT IS_SRVROLEMEMBER('sysadmin') admin,HAS_PERMS_BY_NAME('dbo.TACCOUNT_PW','OBJECT','SELECT') can_read,HAS_PERMS_BY_NAME('dbo.TACCOUNT_PW','OBJECT','UPDATE') can_update,HAS_PERMS_BY_NAME('dbo.TACCOUNT_PW','OBJECT','ALTER') can_alter,HAS_PERMS_BY_NAME('dbo.NativeRegisterAccount','OBJECT','EXECUTE') can_register")[0]
    assert row==dict(admin=0,can_read=0,can_update=0,can_alter=0,can_register=1)
    print('PASS runtime SQL login: execute registration only; no sysadmin/table read/update/alter')
finally:c.close()
hold=db.connect()
try:
    db.query(hold,'USE '+db.ident(info['database']))
    db.query(hold,'BEGIN TRANSACTION;SELECT COUNT(*) n FROM dbo.TACCOUNT_PW WITH(TABLOCKX,HOLDLOCK)')
    body=json.dumps(dict(account='LockProbe',email='',password='TestSecret42',confirmation='TestSecret42')).encode()
    request=urllib.request.Request('https://127.0.0.1:'+str(info['port'])+'/v1/accounts',data=body,headers={'Content-Type':'application/json'})
    context=ssl.create_default_context(cafile=str(ROOT/'runtime/accounts/tls/ca.pem'))
    opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),urllib.request.HTTPSHandler(context=context))
    start=time.monotonic()
    try:opener.open(request,timeout=8);raise AssertionError('lock was ignored')
    except urllib.error.HTTPError as error:assert error.code==503
    assert 4.5<time.monotonic()-start<7
    print('PASS SQL lock timeout: bounded failure within seven seconds')
finally:db.query(hold,'IF @@TRANCOUNT>0 ROLLBACK');hold.close()
