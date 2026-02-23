#coding:utf-8

"""
ID:          issue-8491
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8491
TITLE:       Fix reset of db guid in fixup
DESCRIPTION:
NOTES:
    [02.04.2025] pzotov
    Confirmed regression starting from 6.0.0.647-9fccb55 (21.02.2025); on 6.0.0.640-9b8ac53 result was Ok.
    Confirmed bug on 6.0.0.708-cb06990 (31.03.2025)
    Checked on 6.0.0.710-40651f6 -- all OK.
"""
from pathlib import Path
import locale

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):
    
    with act.db.connect() as con:
        cur = con.cursor()
        cur.execute("select rdb$get_context('SYSTEM', 'DB_GUID') from rdb$database")
        db_guid_ini = cur.fetchall()[0][0]

    act.nbackup(switches=['-L', act.db.db_path], io_enc = locale.getpreferredencoding(), combine_output = True)
    assert act.return_code == 0,f'Attempt to lock DB failed:\n{act.clean_stdout}'
    act.reset()

    act.nbackup(switches=['-F', act.db.db_path], io_enc = locale.getpreferredencoding(), combine_output = True)
    assert act.return_code == 0,f'Attempt to fixup DB failed:\n{act.clean_stdout}'
    act.reset()

    with act.db.connect() as con:
        cur = con.cursor()
        cur.execute("select rdb$get_context('SYSTEM', 'DB_GUID') from rdb$database")
        db_guid_cur = cur.fetchall()[0][0]

    act.reset()
    EXPECTED_MSG = 'Expected: database GUID has changed after fixup.'
    if db_guid_ini != db_guid_cur:
        print(EXPECTED_MSG)
    else:
        print('Database GUID did NOT change:')
        print('Initial:',db_guid_ini)
        print('Current:',db_guid_ini)

    act.expected_stdout = f"""
        {EXPECTED_MSG}
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
