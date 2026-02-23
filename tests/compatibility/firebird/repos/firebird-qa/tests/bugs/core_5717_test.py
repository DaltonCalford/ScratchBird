#coding:utf-8

"""
ID:          issue-5983
ISSUE:       5983
TITLE:       Reject non-constant date/time/timestamp literals
DESCRIPTION:
JIRA:        CORE-5717
FBTEST:      bugs.core_5717
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select date '2018-01-01' from rdb$database;
    select time '10:00:00' from rdb$database;
    select timestamp '2018-01-01 10:00:00' from rdb$database;
    select DATE 'TODAY' from rdb$database;
    select DATE 'TOMORROW' from rdb$database;
    select DATE 'YESTERDAY' from rdb$database;
    select TIME 'NOW' from rdb$database;
    select TIMESTAMP 'NOW' from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    2018-01-01
    10:00:00.0000
    2018-01-01 10:00:00.0000
"""

expected_stderr = """
    Statement failed, SQLSTATE = 22018
    conversion error from string "TODAY"

    Statement failed, SQLSTATE = 22018
    conversion error from string "TOMORROW"

    Statement failed, SQLSTATE = 22018
    conversion error from string "YESTERDAY"

    Statement failed, SQLSTATE = 22018
    conversion error from string "NOW"

    Statement failed, SQLSTATE = 22018
    conversion error from string "NOW"
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
