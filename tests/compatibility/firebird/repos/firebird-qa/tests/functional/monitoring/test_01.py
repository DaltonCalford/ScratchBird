#coding:utf-8

"""
ID:          monitoring-tables-01
TITLE:       Get isolation level of the current transaction
DESCRIPTION:
FBTEST:      functional.monitoring.01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SELECT MON$ISOLATION_MODE
        FROM MON$TRANSACTIONS
WHERE MON$TRANSACTION_ID = CURRENT_TRANSACTION;"""

act = isql_act('db', test_script)

expected_stdout = """
MON$ISOLATION_MODE
==================
                 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
