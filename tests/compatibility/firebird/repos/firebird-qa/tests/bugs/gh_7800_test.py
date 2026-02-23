#coding:utf-8

"""
ID:          issue-7800
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7800
TITLE:       Default publication status is not preserved after backup/restore
DESCRIPTION:
NOTES:
    [01.11.2023] pzotov
    Checked on 6.0.0.101.

    [30.11.2023] pzotov
    Checked on 5.0.0.1283, 4.0.5.3033 after backport (intermediate snapshots 30.11.2023).
"""
from io import BytesIO
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

init_script = """
    alter database enable publication;
    alter database include all to publication;
    commit;
"""
db = db_factory(init=init_script)

#act = python_act('db')

test_script = """
    set list on;
    set count on;
    select rdb$active_flag, rdb$auto_enable from rdb$publications;
"""
act = isql_act('db', test_script)

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    bkp_data = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database = act.db.db_path, backup_stream = bkp_data)
        bkp_data.seek(0)
        srv.database.local_restore(backup_stream = bkp_data, database = act.db.db_path, flags = SrvRestoreFlag.REPLACE)

    expected_stdout = """
       RDB$ACTIVE_FLAG                 1
       RDB$AUTO_ENABLE                 1
       Records affected: 1
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
