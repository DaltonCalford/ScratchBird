#coding:utf-8

"""
ID:          issue-5426
ISSUE:       5426
TITLE:       GBAK restore failed when there is SQL function accessing table and switch -O(NE_AT_A_TIME) is used
DESCRIPTION:
JIRA:        CORE-5143
FBTEST:      bugs.core_5143
"""

import pytest
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvBackupFlag, SrvRestoreFlag

init_script = """
    set term ^;
    create or alter function f1 returns int as begin return 1; end
    ^
    set term ;^
    commit;

    recreate table t1 (id int);
    recreate table t2 (id int);

    set term ^;
    create or alter function f1 returns int
    as
    begin
      return (select count(*) from t1) + (select count(*) from t2);
    end^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

fbk_file = temp_file('core_5143.fbk')

@pytest.mark.version('>=3.0')
def test_1(act: Action, fbk_file: Path):
    with act.connect_server() as srv:
        srv.database.backup(database=act.db.db_path, backup=fbk_file,
                            verbose=True)
        srv.wait()
        srv.database.restore(database=act.db.db_path, backup=fbk_file,
                             flags=SrvRestoreFlag.REPLACE | SrvRestoreFlag.ONE_AT_A_TIME,
                             verbose=True)
        restore_log = srv.readlines()
    # Check
    assert [line for line in restore_log if 'ERROR:' in line.upper()] == []
