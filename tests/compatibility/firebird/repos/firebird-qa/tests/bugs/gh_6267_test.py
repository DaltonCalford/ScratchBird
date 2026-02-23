#coding:utf-8

"""
ID:          issue-6267
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6267
TITLE:       Add transaction info fb_info_tra_snapshot_number [CORE6017]
DESCRIPTION:
    Test verifies ability to use appropriate API info and whether value of
    returned snapshot_number equals to RDB$GET_CONTEXT('SYSTEM', 'SNAPSHOT_NUMBER').
NOTES:
    [22.07.2024] pzotov
    Checked on 6.0.0.396, 5.0.1.1440, 4.0.5.3127
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

db = db_factory()
act = python_act('db')

CUSTOM_TPB = tpb(isolation = Isolation.SNAPSHOT)

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        tx1 = con.transaction_manager(CUSTOM_TPB)
        tx1.begin()
        cur1 = tx1.cursor()
        
        cur1.execute("select RDB$GET_CONTEXT('SYSTEM', 'SNAPSHOT_NUMBER') from rdb$database")
        ctx_sn = int(cur1.fetchone()[0])
        if ctx_sn == tx1.info.snapshot_number:
            print('OK')
        else:
            print(f'MISMATCH: RDB$GET_CONTEXT={ctx_sn}, {tx1.info.snapshot_number=}')
        tx1.commit()

    act.expected_stdout = """
        OK
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
