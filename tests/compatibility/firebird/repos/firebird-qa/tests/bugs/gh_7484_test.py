#coding:utf-8

"""
ID:          issue-7484
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7484
TITLE:       External engine SYSTEM not found
DESCRIPTION:
NOTES:
    [02.03.2023] pzotov
    Confirmed bug on 5.0.0.959 SS (date of build: 26.02.2023), got:
    "External engine SYSTEM not found / -901 / 335545001"

    Checked on 5.0.0.957, 4.0.3.2904 - all OK.
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, TraLockResolution, TraAccessMode, DatabaseError

db = db_factory()
act = python_act('db')

expected_stdout = """
"""

@pytest.mark.encryption
@pytest.mark.version('>=4.0.3')
def test_1(act: Action, capsys):
    sttm = 'select rdb$time_zone_util.database_version() from rdb$database'
    with act.db.connect() as con1, act.db.connect() as con2:
        cur1 = con1.cursor()
        cur2 = con2.cursor()

        try:
            cur1.execute(sttm)
            for r in cur1:
                pass
            con1.close()

            cur2.execute(sttm)
            for r in cur2:
                pass

        except DatabaseError as e:
            print(e.__str__())
            print(e.sqlcode)
            for g in e.gds_codes:
                print(g)
    
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
