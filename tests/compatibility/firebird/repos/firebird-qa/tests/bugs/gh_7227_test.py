#coding:utf-8

"""
ID:          issue-7227
ISSUE:       7227
TITLE:       Dependencies of subroutines are not preserved after backup restore
DESCRIPTION:
NOTES:
    [23.02.2023] pzotov
    Confirmed bug on 5.0.0.573 (04-jul-2022), and on all subsequent snapshots up to 5.0.0.890 (10-jan-2023) - firebird process crashed.
    Checked on 5.0.0.905 (11-jan-2023) - all fine.

    [23.03.2024] pzotov
    Test was not committed in repo for unknown reason. Fixed (after check again on 5.x).
"""
import pytest
from firebird.qa import *
from pathlib import Path
import locale

from firebird.driver import SrvRestoreFlag, SrvRepairFlag
from io import BytesIO

init_script = """
    set term ^;
    create domain domain1 integer
    ^
    create domain domain2 integer
    ^
    create procedure mainproc1 as
        declare procedure subproc1
        as
            declare v domain1;
        begin
        end

        declare function subfunc1 returns integer
        as
            declare v domain2;
        begin
        end
    begin
        -- nop --
    end
    ^
    set term ;^
    commit;
"""
db = db_factory(init = init_script)
act = python_act('db')

db_tmp = db_factory(filename='tmp_gh_7227.restored.fdb', do_not_create=True)

chk_sql = """
    set list on;
    set count on;
    select
        rdb$dependent_name
        ,rdb$depended_on_name
    from rdb$dependencies
    order by rdb$dependent_name;
"""

expected_out = """
    RDB$DEPENDENT_NAME              MAINPROC1
    RDB$DEPENDED_ON_NAME            DOMAIN1

    RDB$DEPENDENT_NAME              MAINPROC1
    RDB$DEPENDED_ON_NAME            DOMAIN2

    Records affected: 2
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action, db_tmp: Database):

    backup = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database = act.db.db_path, backup_stream = backup)
        backup.seek(0)
        srv.database.local_restore(backup_stream = backup, database = db_tmp.db_path, flags = SrvRestoreFlag.REPLACE)

    act.expected_stdout = expected_out
    act.isql(switches=['-q'], use_db = db_tmp, combine_output = True, input = chk_sql, io_enc = locale.getpreferredencoding())

    assert act.clean_stdout == act.clean_expected_stdout
