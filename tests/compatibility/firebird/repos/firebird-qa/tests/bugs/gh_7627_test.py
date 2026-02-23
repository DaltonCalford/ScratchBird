#coding:utf-8

"""
ID:          issue-7627
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7627
TITLE:       The size of the database with big records becomes bigger after backup/restore
DESCRIPTION:
    Test creates table with 'very width' column, adds data to it (padded GUID strings) and makes b/r.
    New size of database (after restore) must not exceed initial one.
NOTES:
    [26.08.2023] pzotov
    Column size must be VERY CLOSE to maximal allowed by implementation.
    On 5.0.0.1070 (date of build 14-jun-2023) bug can be reproduced when 
    column size is 32760, but on size = 32000 this is not so: DB size remains the same.

    Confirmed bug on 5.0.0.1070.
    Checked on 5.0.0.1170, 4.0.4.2979
"""

from io import BytesIO
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

substitutions_1 = [('[ \t]+', ' ')]

PG_SIZE = 8192
init_script = """
    create table test(s varchar(32765));
    insert into test select lpad('', 32765, uuid_to_char(gen_uuid())) from rdb$types;
    commit;
"""

tmp_fbk = temp_file( filename = 'tmp_gh_6709.fbk')
db = db_factory(init = init_script, charset = 'win1251', page_size = PG_SIZE)

act = python_act('db')

expected_stdout = "DB size did not increase after restore."

@pytest.mark.version('>=4.0.3')
def test_1(act: Action, tmp_fbk: Path, capsys):

    db_init_size = Path(act.db.db_path).stat().st_size

    # backup + restore without creating .fbk:
    #
    bkp_data = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database = act.db.db_path, backup_stream = bkp_data)
        bkp_data.seek(0)
        srv.database.local_restore(backup_stream = bkp_data, database = act.db.db_path, flags = SrvRestoreFlag.REPLACE)

    db_curr_size = Path(act.db.db_path).stat().st_size
    if db_init_size - db_curr_size >= 0:
        print(expected_stdout)
    else:
        print(f'DB size increased after restore for {db_curr_size - db_init_size} bytes, from {db_init_size} to {db_curr_size}')

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
