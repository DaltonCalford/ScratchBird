#coding:utf-8

"""
ID:          issue-2694
ISSUE:       2694
TITLE:       GFIX causes BUGCHECK errors with non valid transaction numbers
DESCRIPTION:
JIRA:        CORE-2268
FBTEST:      bugs.core_2268
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('^failed to reconnect to a transaction in database.*', '')])

expected_stderr = """transaction is not in limbo
-transaction 1000000 is in an ill-defined state

"""

@pytest.mark.version('>=2.5')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.gfix(switches=['-commit', '1000000', act.db.dsn])
    assert act.clean_stderr == act.clean_expected_stderr


