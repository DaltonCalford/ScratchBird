#coding:utf-8

"""
ID:          issue-6094
ISSUE:       6094
TITLE:       DDL triggers for some object types (views, exceptions, roles, indexes, domains)
  are lost in backup-restore process
DESCRIPTION:
  We create DDL triggers for all cases that are enumerated in $FB_HOME/doc/sql.extensions/README.ddl_triggers.txt.
  Then query to RDB$TRIGGERS table is applied to database and its results are stored in <log_file_1>.
  After this we do backup and restore to new file, again apply query to RDB$TRIGGERS and store results to <log_file_2>.
  Finally we compare <log_file_1> and <log_file_2> but exclude from comparison lines which starts with to 'BLOB_ID'
  (these are "prefixes" for RDB$TRIGGER_BLR and RDB$TRIGGER_SOURCE).
  Difference should be empty.
JIRA:        CORE-5833
FBTEST:      bugs.core_5833
"""

import pytest
from difflib import unified_diff
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

ddl_list = ['CREATE TABLE', 'ALTER TABLE', 'DROP TABLE',
            'CREATE PROCEDURE', 'ALTER PROCEDURE', 'DROP PROCEDURE',
            'CREATE FUNCTION', 'ALTER FUNCTION', 'DROP FUNCTION',
            'CREATE TRIGGER', 'ALTER TRIGGER', 'DROP TRIGGER',
            'CREATE EXCEPTION', 'ALTER EXCEPTION', 'DROP EXCEPTION',
            'CREATE VIEW', 'ALTER VIEW', 'DROP VIEW',
            'CREATE DOMAIN', 'ALTER DOMAIN', 'DROP DOMAIN',
            'CREATE ROLE', 'ALTER ROLE', 'DROP ROLE',
            'CREATE SEQUENCE', 'ALTER SEQUENCE', 'DROP SEQUENCE',
            'CREATE USER', 'ALTER USER', 'DROP USER',
            'CREATE INDEX', 'ALTER INDEX', 'DROP INDEX',
            'CREATE COLLATION', 'DROP COLLATION', 'ALTER CHARACTER SET',
            'CREATE PACKAGE', 'ALTER PACKAGE', 'DROP PACKAGE',
            'CREATE PACKAGE BODY', 'DROP PACKAGE BODY']

test_script = """
    set blob all;
    set list on;
    set count on;
    select rdb$trigger_name, rdb$trigger_type, rdb$trigger_source as blob_id_for_trg_source, rdb$trigger_blr as blob_id_for_trg_blr
    from rdb$triggers
    where rdb$system_flag is distinct from 1
    order by 1;
"""

fbk_file = temp_file('tmp_5833.fbk')
fdb_file = temp_file('tmp_5833.fdb')

@pytest.mark.version('>=3.0')
def test_1(act: Action, fbk_file: Path, fdb_file: Path):
    script = ['set bail on;', 'set term ^;']
    # Initial DDL: create all triggers
    for item in ddl_list:
        for evt_time in ['before', 'after']:
            script.append(f"recreate trigger trg_{evt_time}_{item.replace(' ', '_').lower()} active {evt_time} {item.lower()} as")
            script.append("    declare c rdb$field_name;")
            script.append("begin")
            script.append("    c = rdb$get_context('DDL_TRIGGER', 'OBJECT_NAME');")
            script.append("end ^")
            script.append("")
    script.append("set term ;^")
    script.append("commit;")
    act.isql(switches=[], input='\n'.join(script))
    # Query RDB$TRIGGERS before b/r:
    act.reset()
    act.isql(switches=[], input=test_script)
    meta_before = [line for line in act.clean_stdout.splitlines() if not line.startswith('BLOB_ID_FOR_TRG')]
    # B/S
    act.reset()
    act.gbak(switches=['-b', act.db.dsn, str(fbk_file)])
    act.reset()
    act.gbak(switches=['-c', str(fbk_file), act.get_dsn(fdb_file)])
    # Query RDB$TRIGGERS after b/r:
    act.reset()
    act.isql(switches=[act.get_dsn(fdb_file)], input=test_script, connect_db=False)
    meta_after = [line for line in act.clean_stdout.splitlines() if not line.startswith('BLOB_ID_FOR_TRG')]
    # Check
    assert list(unified_diff(meta_before, meta_after)) == []
