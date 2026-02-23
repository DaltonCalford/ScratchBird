#coding:utf-8

"""
ID:          issue-4410
ISSUE:       4410
TITLE:       Wrong error message
DESCRIPTION: Wrong value of expected length in the string right truncation error
JIRA:        CORE-4082
FBTEST:      bugs.core_4082
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    execute block as
        declare u varchar(14);
    begin
        u = gen_uuid();
    end
    ^
"""

act = isql_act('db', test_script, substitutions=[('-At block line: [\\d]+, col: [\\d]+', '-At block line')])

expected_stderr = """
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 14, actual 16
    -At block line: 4, col: 9
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

