#!/usr/bin/env python3
"""C5 read-only SQL snapshot of the rows that character creation and deletion touch.

    deps/python/database/bin/python tools/c5_sql_snapshot.py LABEL USER_ID [USER_ID...]

Runs only SELECT statements inside a transaction that is rolled back. For the
given account IDs it reads TGLOBAL_GSP TUSERINFOTABLE/TALLCHARTABLE and every
TGAME_GSP table that TCreateChar or TDeleteChar writes (GAME_TABLES, with the
key column per table; tables missing in the schema are listed). Output: runtime/legacy/c5/sql/LABEL.json (private: it can hold
account data; never commit it). Prints table row counts only.
"""
import datetime, decimal, json, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/database'))
import native_cluster_db as db  # noqa: E402

# Every TGAME_GSP table that TCreateChar or TDeleteChar (TGAME_GSP) writes,
# with the column that ties its rows to the test accounts (C5 review R-C24):
# char = dwCharID; item = dwOwnerID of a character (bOwnerType 0, as
# TDeleteChar selects them); owner = dwOwnerID; user = dwUserID; the friend
# and soulmate tables also match the other side of the relation.
GAME_TABLES = {
    'TCHARTABLE': 'user', 'TCABINETTABLE': 'char', 'TCASTLEAPPLICANTTABLE': 'char', 'TDUELCHARTABLE': 'char',
    'TDUELSCORETABLE': 'char', 'TEXPITEMTABLE': 'char', 'TFRIENDGROUPTABLE': 'char', 'TFRIENDTABLE': 'char|dwFriendID',
    'THOTKEYTABLE': 'char', 'TINVENTABLE': 'char', 'TITEMTABLE': 'item', 'TITEMUSEDTABLE': 'char', 'TPOSTTABLE': 'char',
    'TPROTECTEDTABLE': 'char', 'TPVPOINTTABLE': 'char', 'TPVPRECENTTABLE': 'char', 'TPVPRECORDTABLE': 'char',
    'TQUESTTABLE': 'char', 'TQUESTTERMTABLE': 'char', 'TRECALLMAINTAINTABLE': 'char', 'TRECALLMONTABLE': 'owner',
    'TSKILLMAINTAINTABLE': 'char', 'TSKILLTABLE': 'char', 'TSOULMATETABLE': 'char|dwTarget', 'TTEMPCABINETTABLE': 'char',
    'TTEMPEXPITEMTABLE': 'char', 'TTEMPINVENTABLE': 'char', 'TTEMPITEMTABLE': 'item', 'TTEMPITEMUSEDTABLE': 'char',
    'TTEMPSKILLMAINTAINTABLE': 'char', 'TTEMPSKILLTABLE': 'char', 'TPETTABLE': 'user', 'TTITLETABLE': 'char'}


def plain(v):
    if isinstance(v, (datetime.datetime, datetime.date)): return v.isoformat()
    if isinstance(v, decimal.Decimal): return str(v)
    if isinstance(v, (bytes, bytearray)): return v.hex()
    return v


def columns(c, table):
    return [r['COLUMN_NAME'] for r in db.query(c, 'SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_NAME=%s ORDER BY ORDINAL_POSITION', (table,))]


def main():
    if len(sys.argv) < 3: raise SystemExit(__doc__)
    label, users = sys.argv[1], [int(u) for u in sys.argv[2:]]
    out = {'label': label, 'users': users, 'taken': datetime.datetime.now().isoformat(timespec='seconds'), 'tables': {}}
    c = db.connect()
    begun = False
    try:
        db.query(c, 'BEGIN TRANSACTION')
        begun = True
        marks = ','.join(['%s'] * len(users))
        db.query(c, 'USE [TGLOBAL_GSP]')
        out['tables']['TGLOBAL.TUSERINFOTABLE'] = db.query(c, f'SELECT * FROM TUSERINFOTABLE WHERE dwUserID IN ({marks})', tuple(users))
        out['tables']['TGLOBAL.TALLCHARTABLE'] = db.query(c, f'SELECT * FROM TALLCHARTABLE WHERE dwUserID IN ({marks}) ORDER BY dwCharID', tuple(users))
        db.query(c, 'USE [TGAME_GSP]')
        chars = [r['dwCharID'] for r in db.query(c, f'SELECT dwCharID FROM TCHARTABLE WHERE dwUserID IN ({marks})', tuple(users))]
        chars += [r['dwCharID'] for r in out['tables']['TGLOBAL.TALLCHARTABLE'] if r['dwCharID'] not in chars]
        out['chars'] = sorted(chars)
        for table, rule in GAME_TABLES.items():
            cols = columns(c, table)
            if not cols:
                out.setdefault('missing_tables', []).append(table); continue
            um = ','.join(['%s'] * len(users))
            cm = ','.join(['%s'] * len(chars)) if chars else 'NULL'
            order = ', '.join(cols[:4])
            if rule == 'user':
                rows = db.query(c, f'SELECT * FROM {table} WHERE dwUserID IN ({um}) ORDER BY {order}', tuple(users))
            elif not chars:
                rows = []
            elif rule == 'item':
                rows = db.query(c, f'SELECT * FROM {table} WHERE bOwnerType=0 AND dwOwnerID IN ({cm}) ORDER BY {order}', tuple(chars))
            elif rule == 'owner':
                rows = db.query(c, f'SELECT * FROM {table} WHERE dwOwnerID IN ({cm}) ORDER BY {order}', tuple(chars))
            else:
                other = rule.split('|')[1] if '|' in rule else None
                where = f'dwCharID IN ({cm})' + (f' OR {other} IN ({cm})' if other else '')
                rows = db.query(c, f'SELECT * FROM {table} WHERE {where} ORDER BY {order}', tuple(chars) * (2 if other else 1))
            out['tables']['TGAME.' + table] = rows
            out.setdefault('columns', {})['TGAME.' + table] = cols
    finally:
        try:
            if begun: db.query(c, 'ROLLBACK TRANSACTION')
        finally:
            c.close()
    for k, rows in out['tables'].items():
        out['tables'][k] = [{n: plain(v) for n, v in r.items()} for r in rows]
    path = ROOT / 'runtime/legacy/c5/sql' / (label + '.json')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(out, indent=1, sort_keys=True))
    path.chmod(0o600)
    print(label, 'chars', len(out['chars']), ' '.join(f'{k.split(".")[1]}={len(v)}' for k, v in out['tables'].items() if v))


if __name__ == '__main__':
    main()
