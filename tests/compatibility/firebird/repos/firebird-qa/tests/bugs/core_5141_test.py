#coding:utf-8

"""
ID:          issue-5424
ISSUE:       5424
TITLE:       Field definition allows several NOT NULL clauses
DESCRIPTION:
JIRA:        CORE-5141
FBTEST:      bugs.core_5141
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Confirmed:
    -- * wrong behavour: WI-V3.0.0.32378 Firebird 3.0
    -- * proper result (compiler errror): WI-T4.0.0.32390 Firebird 4.0.
    recreate table t1 (a integer not null not null not null);
    recreate table t2 (a integer unique not null not null references t2(a));
    recreate table t3 (a integer unique not null references t2(a) not null);
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    SQL error code = -637
    -duplicate specification of NOT NULL - not supported

    Statement failed, SQLSTATE = 42000
    SQL error code = -637
    -duplicate specification of NOT NULL - not supported

    Statement failed, SQLSTATE = 42000
    SQL error code = -637
    -duplicate specification of NOT NULL - not supported
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
