#coding:utf-8

"""
ID:          issue-2311
ISSUE:       2311
TITLE:       Signature mismatch when creating package body on identical packaged procedure header
DESCRIPTION:
  Bug existed because backup/restore process changed value of RDB$PROCEDURE_PARAMETERS.RDB$NULL_FLAG
  for procedure parameter from NULL to 0 (zero).
  Test creates trivial package and stores its package body in variable that will be used after b/r.
  Then we do backup / restore and attempt to apply this stored package body again, see 'sql_pk_body'.

  Confirmed bug on: 4.0.0.1766,  3.0.6.33247. Attempt to apply 'recreate package ...' with the same SQL code fails with:
    Statement failed, SQLSTATE = 42000 / ... / -Procedure ... has a signature mismatch on package body ...
  Bug was partially fixed in snapshots 4.0.0.1782 and 3.0.6.33252: problem remained if procedure parameter was of built-in
  datatype rather than domain (i.e. this parameter type was TIMESTAMP or INT etc, instead of apropriate domain).

  Completely fixed in snapshots 4.0.0.1783 and 3.0.6.33254 (checked 23.02.2020).
  Added special check for parameter that is declared of built-in datatype rather than domain.
JIRA:        CORE-6250
FBTEST:      bugs.core_6250
"""

import pytest
from io import BytesIO
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

sql_pk_body = """
    set term ^;
    recreate package body pg_test1 as
    begin
        procedure sp_test1 ( a_since dm_dts ) as begin end
    end
    ^
    recreate package body pg_test2 as
    begin
        procedure sp_test2 ( a_since timestamp ) as begin end
    end
    ^
    set term ;^
"""

sql_init = f"""
    create domain dm_dts timestamp;
    commit;
    set term ^;
    create or alter package pg_test1 as
    begin
      procedure sp_test1 ( a_since dm_dts ) ;
    end
    ^
    create or alter package pg_test2 as
    begin
      procedure sp_test2 ( a_since timestamp ) ;
    end
    ^
    set term ;^

    {sql_pk_body}

    commit;
"""

fdb_restored = temp_file('tmp_6250_restored.fdb')

@pytest.mark.version('>=3.0.6')
def test_1(act: Action, fdb_restored: Path):
    act.isql(switches=[], input=sql_init)
    backup = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(database=fdb_restored, backup_stream=backup)
    #
    act.reset()
    act.isql(switches=['-q', act.get_dsn(fdb_restored)], input=sql_pk_body, connect_db=False)
    assert act.clean_stdout == act.clean_expected_stdout
