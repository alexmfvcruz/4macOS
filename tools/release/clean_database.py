#!/usr/bin/env python3
"""Export the clean release databases (C8 server package, work item 4).

Usage: deps/python/database/bin/python tools/release/clean_database.py

Restores the supplied audit backups into two fixed staging databases, removes
accounts, characters, guilds, logs, the original deployment topology,
credentials and database users (rules: research/c8-packaging-plan.md,
"Clean database rules"), and writes release/Server/database/
{TGLOBAL_GSP,TGAME_GSP}.bak plus manifest.json.

Checks before the files replace the release folder: only listed content
tables keep rows; no owner, login or address column keeps a value; the
backup bytes contain none of the removed text values (Latin-1 or UTF-16).
The audit sources and the running Native cluster databases are not changed;
the staging databases and temporary container files are removed at the end.
"""
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/database'))
from native_cluster_db import connect, fingerprint, ident, literal, query, sources  # noqa: E402

OUT = ROOT / 'release/Server/database'
DOCKER = ['docker', '--context', 'colima-4story-audit']
CONTAINER = '4story-audit-sql'
DATA = '/var/opt/mssql/data/'
# Release name and staging name per kind. Constants: nothing else is dropped.
STAGING = {'TGLOBAL': ('TGLOBAL_GSP', '4Story_Release_TGLOBAL'), 'TGAME': ('TGAME_GSP', '4Story_Release_TGAME')}

CLEAR = {
    'TGLOBAL': ['TACCOUNT', 'TCASHITEMBUYTABLE', 'TEVENT042501TL', 'TLOG', 'TLogTest', 'TMANAGER',
                'TPCBANGPLAYTABLE', 'TRESERVEDNAME', 'TTESTLOGINUSER', 'TUSERINFOTABLE', 'TUSERPROTECTED_old',
                # Original deployment topology; Install writes this Mac's rows.
                'TSERVER', 'TIPADDR', 'TNETWORK', 'TMACHINE'],
    'TGAME': ['STATISTICACCOUNT', 'STATISTICTEMP', 'TCOMPANIONITEMTABLE', 'TCUSTOMCLOAKTABLE',
              'TSAVEDCUSTOMCLOAKTABLE', 'TEST', 'TGUILDSTATSTABLE', 'TGUILDTABLE_copy', 'TLASTCOMPANIONTABLE',
              'tgame.TLASTMONTHPOINTTABLE', 'tgame.TLASTTOTALPOINTTABLE', 'TLOCALOCCUPYTABLE', 'TMEDALS',
              'TRANKING', 'TTITLETABLE'],
}
RESET = {
    'TGLOBAL': ["UPDATE TGROUP SET szDSN='',szUserID='',szPasswd=''"],
    'TGAME': ["UPDATE TCASTLETABLE SET dwGuildID=0,szHero=''",
              "UPDATE TLOCALTABLE SET dwGuild=0,szHero=''",
              'UPDATE TTAXTABLE SET dwMoney=0'],
}
# Tables that may keep rows, besides every table whose name ends with CHART.
KEEP = {
    'TGLOBAL': {'dtproperties', 'releaseDate', 'TCASHITEMPRICETABLE', 'TCHANNEL', 'TGROUP', 'TKEEPINGNAME',
                'TSVRTYPE', 'TUSER_INTERFACE'},
    'TGAME': {'dtproperties', 'TCASTLETABLE', 'TCHANGEDITEM', 'TDBITEMINDEXTABLE', 'TERASEITEM',
              'THELPMESSAGETABLE', 'TLOCALTABLE', 'TMISSIONTABLE', 'TSKILLDATA', 'TSKILLREWARD', 'TSKYGARDENTABLE',
              'TSTARTHOTKEY', 'TSTARTRECALL', 'TSTARTSKILL', 'TTAXTABLE', 'TTNMTEVENTSCHEDULETABLE',
              'TTNMTEVENTTIMETABLE',
              # Backup copies of content charts in the supplied data.
              'TBOWITEMCHART1', 'TITEMCHART_bak', 'TTOURNAMENTREWARDCHART_weap'},
}
# Database users of the original deployment, and the schemas they own.
USERS = ['tgame', 'tglobal', 'www-data']
# Removed values (SHA-256 of UTF-8) that survive only as unreachable bytes in
# system-table pages of TGLOBAL even after ghost and free-space cleanup: a
# 4-digit number from TLogTest.szLOG and a 5-character original server name
# from TSERVER.szName. Neither is personal; both are in the supplied backups
# committed under source/4Story 5.0 Database/. Any other value fails the scan.
ACCEPTED_RESIDUE = {'TGLOBAL': {'85d6385b945c0d602103db39b0b654b2af93b5127938e26a959c123f0789b948',
                                '8a2043a43737285c757e2aa60c37b8ec6fb9d9acb1c8d9ed7cdd9777415c80b4'},
                    'TGAME': set()}
# Columns that must hold no value after cleaning (owner, login, address data).
OWNER = ("c.name LIKE '%CharID' OR c.name LIKE '%UserID' OR c.name IN "
         "('dwGuildID','dwGuild','szPasswd','szIPAddr','szPriAddr','szHero','szOpratorCharID')")


def table(name):
    schema, _, base = name.rpartition('.')
    return ident(schema or 'dbo') + '.' + ident(base)


def removed_text(conn, kind):
    """Text values (4+ characters) that the cleaning removes from this database."""
    values = set()
    columns = [(name, col['name']) for name in CLEAR[kind] for col in query(
        conn, "SELECT c.name FROM sys.columns c JOIN sys.types t ON t.user_type_id=c.user_type_id "
              "WHERE c.object_id=OBJECT_ID(%s) AND t.name IN('varchar','nvarchar','char','nchar')",
        (table(name),))]
    columns += {'TGLOBAL': [('TGROUP', 'szDSN'), ('TGROUP', 'szUserID'), ('TGROUP', 'szPasswd')],
                'TGAME': [('TCASTLETABLE', 'szHero'), ('TLOCALTABLE', 'szHero')]}[kind]
    for name, col in columns:
        for row in query(conn, f'SELECT DISTINCT RTRIM({ident(col)}) AS v FROM {table(name)} '
                               f'WHERE LEN({ident(col)})>=4'):
            values.add(row['v'])
    return values | {'www-data'}


def kept_tables(conn):
    """Tables with rows, counted exactly (sys.partitions counts can be stale)."""
    tables = query(conn, 'SELECT s.name AS s,t.name AS t FROM sys.tables t JOIN sys.schemas s ON s.schema_id=t.schema_id')
    return {(r['s'], r['t']) for r in tables
            if query(conn, f"SELECT TOP 1 1 AS x FROM {ident(r['s'])}.{ident(r['t'])}")}


def owner_values(conn):
    """Columns that keep a non-empty, non-zero owner, login or address value."""
    found = []
    columns = query(conn, 'SELECT s.name AS s,t.name AS t,c.name AS c FROM sys.tables t '
                          'JOIN sys.schemas s ON s.schema_id=t.schema_id JOIN sys.columns c ON c.object_id=t.object_id '
                          'WHERE ' + OWNER)
    for col in columns:
        name = ident(col['s']) + '.' + ident(col['t'])
        value = f"LTRIM(RTRIM(CAST({ident(col['c'])} AS nvarchar(4000))))"
        n = query(conn, f"SELECT COUNT(*) AS n FROM {name} WHERE NULLIF({value},'') IS NOT NULL AND {value}<>'0'")[0]['n']
        if n:
            found.append(f"{col['t']}.{col['c']}={n}")
    return found


def catalog_text(conn):
    """Words of module text and object, column and schema names: legitimate in any backup."""
    parts = [r['v'] or '' for r in query(conn, 'SELECT definition AS v FROM sys.sql_modules')]
    parts += [r['v'] for r in query(conn, 'SELECT name AS v FROM sys.objects UNION SELECT name FROM sys.columns '
                                          'UNION SELECT name FROM sys.schemas')]
    return set(re.findall(r'[^\s\[\]().,;=\'"+*/<>]+', '\n'.join(parts).lower()))


def leaked(path, values, allowed, kind):
    data = path.read_bytes()
    hits = []
    for value in sorted(values):
        # Skip only a value that is a whole word of the catalog text.
        if value.lower() in allowed or hashlib.sha256(value.encode('utf-8')).hexdigest() in ACCEPTED_RESIDUE[kind]:
            continue
        for encoding in ('latin-1', 'utf-16-le'):
            try:
                needle = value.encode(encoding)
            except UnicodeEncodeError:
                continue
            if needle in data:
                hits.append(f'{len(value)}-character value ({encoding})')
                break
    return hits


def docker(*args):
    subprocess.run(DOCKER + list(args), check=True, stdout=subprocess.DEVNULL)


def drop_staging(conn, stage):
    query(conn, 'IF @@TRANCOUNT>0 ROLLBACK')
    query(conn, 'USE master')
    query(conn, f'IF DB_ID({literal(stage)}) IS NOT NULL BEGIN ALTER DATABASE {ident(stage)} '
                f'SET SINGLE_USER WITH ROLLBACK IMMEDIATE; DROP DATABASE {ident(stage)}; END')


def export(conn, kind, src, work):
    release, stage = STAGING[kind]
    copy, target = DATA + stage + '-source.bak', DATA + stage + '-release.bak'
    try:
        query(conn, 'USE ' + ident(src))
        removed = removed_text(conn, kind)
        query(conn, 'USE master')
        query(conn, f'BACKUP DATABASE {ident(src)} TO DISK={literal(copy)} WITH COPY_ONLY, INIT, CHECKSUM')
        moves = []
        for index, file in enumerate(query(conn, 'RESTORE FILELISTONLY FROM DISK=' + literal(copy))):
            suffix = 'ldf' if file['Type'] == 'L' else 'mdf'
            moves.append(f"MOVE {literal(file['LogicalName'])} TO {literal(f'{DATA}{stage}-{index}.{suffix}')}")
        drop_staging(conn, stage)
        query(conn, f"RESTORE DATABASE {ident(stage)} FROM DISK={literal(copy)} WITH {', '.join(moves)}, RECOVERY, CHECKSUM")
        query(conn, f'ALTER DATABASE {ident(stage)} SET RECOVERY SIMPLE')
        query(conn, 'USE ' + ident(stage))
        # DELETE leaves emptied heap pages allocated, and an in-place UPDATE
        # leaves the old bytes in the page; a backup copies both. TRUNCATE
        # releases the pages; every other changed table is rebuilt into new pages.
        statements, rebuild = [], [statement.split()[1] for statement in RESET[kind]]
        for name in CLEAR[kind]:
            if query(conn, 'SELECT TOP 1 1 AS x FROM sys.foreign_keys WHERE referenced_object_id=OBJECT_ID(%s)',
                     (table(name),)):
                statements.append(f'DELETE FROM {table(name)}')
                rebuild.append(name)
            else:
                statements.append(f'TRUNCATE TABLE {table(name)}')
        statements += RESET[kind]
        for user in USERS:
            statements.append(f"IF SCHEMA_ID({literal(user)}) IS NOT NULL ALTER AUTHORIZATION ON SCHEMA::{ident(user)} TO dbo")
            statements.append(f'IF USER_ID({literal(user)}) IS NOT NULL DROP USER {ident(user)}')
        query(conn, 'SET XACT_ABORT ON; BEGIN TRANSACTION; ' + '; '.join(statements) + '; COMMIT;')
        for name in rebuild:
            query(conn, f'ALTER TABLE {table(name)} REBUILD')
            if query(conn, 'SELECT TOP 1 1 AS x FROM sys.indexes WHERE object_id=OBJECT_ID(%s) AND index_id>0',
                     (table(name),)):
                query(conn, f'ALTER INDEX ALL ON {table(name)} REBUILD')
        # Column statistics keep up to 200 sample values per column (names,
        # passwords) after the rows are gone. Drop the automatic ones (SQL
        # Server recreates them on demand) and recompute the rest from the
        # remaining rows.
        for stat in query(conn, 'SELECT OBJECT_SCHEMA_NAME(s.object_id) AS sc,OBJECT_NAME(s.object_id) AS t,s.name '
                                'FROM sys.stats s JOIN sys.tables t ON t.object_id=s.object_id WHERE s.auto_created=1'):
            query(conn, f"DROP STATISTICS {ident(stat['sc'])}.{ident(stat['t'])}.{ident(stat['name'])}")
        for t in query(conn, 'SELECT s.name AS s,t.name AS t FROM sys.tables t JOIN sys.schemas s ON s.schema_id=t.schema_id'):
            query(conn, f"UPDATE STATISTICS {ident(t['s'])}.{ident(t['t'])} WITH FULLSCAN")
        extra = sorted(f'{s}.{t}' for s, t in kept_tables(conn)
                       if not t.upper().endswith('CHART') and t not in KEEP[kind])
        if extra:
            raise RuntimeError(f'{kind}: tables outside the keep list have rows: ' + ', '.join(extra))
        left = owner_values(conn)
        if left:
            raise RuntimeError(f'{kind}: owner values remain: ' + ', '.join(left))
        query(conn, 'CHECKPOINT')
        for file in query(conn, 'SELECT name FROM sys.database_files'):
            query(conn, f"DBCC SHRINKFILE({literal(file['name'])}, 1) WITH NO_INFOMSGS")
        allowed = catalog_text(conn)
        query(conn, 'DBCC UPDATEUSAGE(0) WITH NO_INFOMSGS, COUNT_ROWS')
        rows = query(conn, 'SELECT SUM(p.rows) AS n FROM sys.tables t JOIN sys.partitions p '
                           'ON p.object_id=t.object_id AND p.index_id IN(0,1)')[0]['n']
        # Deleted rows, including the statistics rows in sys.sysobjvalues, stay
        # on their pages as ghost records until ghost cleanup removes them.
        # Force that, then overwrite the free space of every page.
        for _ in range(2):
            query(conn, 'USE ' + ident(stage))
            query(conn, 'DBCC FORCEGHOSTCLEANUP WITH NO_INFOMSGS')
            query(conn, 'CHECKPOINT')
            query(conn, 'USE master')
            query(conn, f'EXEC sp_clean_db_free_space {literal(stage)}')
        # Uncompressed, so the byte scan below sees the stored text.
        query(conn, f'BACKUP DATABASE {ident(stage)} TO DISK={literal(target)} WITH COPY_ONLY, INIT, CHECKSUM')
        query(conn, f'RESTORE VERIFYONLY FROM DISK={literal(target)} WITH CHECKSUM')
        path = work / (release + '.bak')
        docker('cp', f'{CONTAINER}:{target}', str(path))
        hits = leaked(path, removed, allowed, kind)
        if hits:
            raise RuntimeError(f'{kind}: the backup still contains removed text: ' + ', '.join(hits))
        print(f'{release}: {path.stat().st_size // 1024} KiB, {rows} rows; checked {len(removed)} removed values',
              flush=True)
        return {'file': path.name, 'bytes': path.stat().st_size, 'rows': int(rows),
                'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                'cleared': CLEAR[kind], 'reset': RESET[kind], 'dropped_users': USERS}
    finally:
        try:
            drop_staging(conn, stage)
        except Exception:
            print('Could not drop staging database ' + stage, file=sys.stderr)
        subprocess.run(DOCKER + ['exec', CONTAINER, 'rm', '-f', copy, target], stdout=subprocess.DEVNULL)


def main():
    src = sources()
    conn = connect()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='.database-', dir=OUT.parent))
    try:
        before = {kind: fingerprint(conn, db) for kind, db in src.items()}
        manifest = {'source': 'supplied 4Story 5.0 database backups', 'databases': {}}
        for kind in STAGING:
            manifest['databases'][STAGING[kind][0]] = export(conn, kind, src[kind], work)
        if before != {kind: fingerprint(conn, db) for kind, db in src.items()}:
            raise RuntimeError('Audit source content changed')
        (work / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        work.chmod(0o755)
        for path in work.iterdir():
            path.chmod(0o644)
        # Replace the release folder only after every check passed.
        old = OUT.with_name('.database-old')
        shutil.rmtree(old, ignore_errors=True)
        if OUT.exists():
            OUT.rename(old)
        work.rename(OUT)
        shutil.rmtree(old, ignore_errors=True)
        print('Wrote ' + str(OUT.relative_to(ROOT)) + '; audit sources unchanged.')
    finally:
        conn.close()
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        raise SystemExit('clean_database: ' + (str(error) if isinstance(error, RuntimeError)
                                                else type(error).__name__ + ' (driver details suppressed)'))
