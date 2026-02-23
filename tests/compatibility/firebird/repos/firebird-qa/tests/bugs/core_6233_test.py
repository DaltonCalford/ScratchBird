#coding:utf-8

"""
ID:          issue-6477
ISSUE:       6477
TITLE:       Wrong dependencies of stored function on view after backup and restore
DESCRIPTION:
  We make backup of this test DB and restore it to other name using PIPE mechanism
  in order to skip creation of unneeded .fbk file
  See: https://docs.python.org/2/library/subprocess.html#replacing-shell-pipeline
JIRA:        CORE-6233
FBTEST:      bugs.core_6233
"""

import pytest
from io import BytesIO
from pathlib import Path
from firebird.qa import *

init_script = """
    set bail on;
    create or alter procedure p1 as begin end;
    create or alter function f1 returns integer as begin end;
    commit;

    set term ^;
    create or alter view v1 as
      select 1 as n from rdb$database
    ^

    create or alter function f1 returns integer as
      declare ret integer;
    begin
      select n from v1 into ret;
      return ret;
    end
    ^

    create or alter procedure p1 returns (ret integer) as
    begin
      select n from v1 into ret;
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

test_script = """
    set list on;
    set count on;
    select
       RDB$DEPENDENT_NAME as dep_name
      ,RDB$DEPENDED_ON_NAME as dep_on_name
    from rdb$dependencies
    order by 1,2;
"""

expected_stdout = """
    DEP_NAME                        F1
    DEP_ON_NAME                     V1
    DEP_NAME                        F1
    DEP_ON_NAME                     V1
    DEP_NAME                        P1
    DEP_ON_NAME                     V1
    DEP_NAME                        P1
    DEP_ON_NAME                     V1
    DEP_NAME                        V1
    DEP_ON_NAME                     RDB$DATABASE
    Records affected: 5
"""

fdb_restored = temp_file('core_6233_restored.fdb')

@pytest.mark.version('>=3.0.6')
def test_1(act: Action, fdb_restored: Path):
    with act.connect_server() as srv:
        backup = BytesIO()
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(database=fdb_restored, backup_stream=backup)
    #
    act.expected_stdout = expected_stdout
    act.isql(switches=[act.get_dsn(fdb_restored)], input=test_script, connect_db=False)
    assert act.clean_stdout == act.clean_expected_stdout
