#!/usr/bin/env python3
"""Create a disposable *new* database and verify account tests. Never reset/drop.
Use the existing native-cluster venv (pymssql). No credentials printed.
"""
import argparse
from datetime import datetime
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[3]
MAIN = ROOT.parent.parent if ROOT.name == 'codex' and ROOT.parent.name == '.local' else ROOT
sys.path.insert(0,str(MAIN/'tools/database'))
import native_cluster_db as db
p=argparse.ArgumentParser(description=__doc__);p.add_argument('mode',choices=['prepare','verify']);a=p.parse_args()
record=ROOT/'runtime/accounts-test-db.txt'
c=db.connect()
try:
    if a.mode=='prepare':
        name='FourStory_Accounts_Test_'+datetime.now().strftime('%Y%m%d_%H%M%S_%f')
        db.query(c,'CREATE DATABASE '+db.ident(name));db.query(c,'USE '+db.ident(name))
        db.query(c,"CREATE TABLE dbo.TACCOUNT_PW(szUserID varchar(50) COLLATE Latin1_General_CI_AS_KS NOT NULL,szPasswd varchar(255),bCheck tinyint,dFirstLogin smalldatetime,dLastLogin smalldatetime,dwUserID int IDENTITY(1,1) NOT NULL PRIMARY KEY); CREATE TABLE dbo.TUSERINFOTABLE(dwUserID int PRIMARY KEY,bCanCreateCharCount tinyint NOT NULL,bAgreement tinyint NOT NULL,dCabinetUse smalldatetime);INSERT TACCOUNT_PW(szUserID,szPasswd) VALUES('LegacyAccount','preserve-existing-hash');INSERT TUSERINFOTABLE(dwUserID,bCanCreateCharCount,bAgreement) VALUES(31167,2,0);")
        db.query(c,(ROOT/'native/accounts/sql/001-registration.sql').read_text())
        db.query(c,"CREATE TRIGGER RejectTestEmail ON dbo.NativeAccountProfile AFTER INSERT AS BEGIN IF EXISTS(SELECT 1 FROM inserted WHERE Email='rollback@example.test') THROW 51010,'Intentional isolated-test rollback',1; END;")
        record.write_text(name)
        print('Prepared '+name+'; no existing database reset or removed')
    else:
        name=record.read_text().strip()
        assert name.startswith('FourStory_Accounts_Test_') and name.replace('_','').isalnum()
        db.query(c,'USE '+db.ident(name))
        assert db.query(c,"SELECT COUNT(*) n FROM dbo.TACCOUNT_PW WHERE szUserID='LegacyAccount' AND szPasswd='preserve-existing-hash'")[0]['n']==1
        assert db.query(c,"SELECT COUNT(*) n FROM dbo.TACCOUNT_PW WHERE szUserID='RollbackPlayer'")[0]['n']==0
        assert db.query(c,"SELECT COUNT(*) n FROM dbo.TUSERINFOTABLE WHERE dwUserID=31167 AND bCanCreateCharCount=2 AND bAgreement=0")[0]['n']==1
        assert db.query(c,"SELECT COUNT(*) n FROM dbo.TACCOUNT_PW WHERE szUserID='NewPlayer' AND dwUserID>31167 AND szPasswd=CONVERT(varchar(40),HASHBYTES('SHA1','NeverLogThis42'),2)")[0]['n']==1
        assert db.query(c,"SELECT COUNT(*) n FROM dbo.NativeAccountProfile p JOIN dbo.TACCOUNT_PW a ON a.dwUserID=p.dwUserID WHERE a.szUserID='NewPlayer' AND p.Email='player@example.com'")[0]['n']==1
        assert db.query(c,"SELECT COUNT(*) n FROM dbo.TUSERINFOTABLE i WHERE i.dwUserID>31167 AND NOT EXISTS(SELECT 1 FROM dbo.TACCOUNT_PW a WHERE a.dwUserID=i.dwUserID)")[0]['n']==0
        print('PASS legacy hash, orphan metadata, new SHA1/e-mail, rollback and absence of partial rows')
finally:c.close()
