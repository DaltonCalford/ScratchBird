#coding:utf-8

"""
ID:          issue-6680
ISSUE:       6680
TITLE:       Unexpectedly different text of message for parameterized expression starting from second run
DESCRIPTION:
  Previous title: "SET SQLDA_DISPLAY ON: different text of message for parameterized expression starting from second run"
JIRA:        CORE-6447
FBTEST:      bugs.core_6447
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display on;
    select 1 from rdb$database where current_connection = ? and current_transaction = ?;
    select 1 from rdb$database where current_connection = ? and current_transaction = ?;
"""

act = isql_act('db', test_script,
               substitutions=[('^((?!sqltype|SQLSTATE|(e|E)rror|number|SQLDA).)*$', ''),
                              ('[ \t]+', ' ')])

expected_stdout = """
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
    02: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
    01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4

    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
    02: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
    01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
"""

expected_stderr = """
    Statement failed, SQLSTATE = 07002
    Dynamic SQL Error
    -SQLDA error
    -No SQLDA for input values provided

    Statement failed, SQLSTATE = 07002
    Dynamic SQL Error
    -SQLDA error
    -No SQLDA for input values provided
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
