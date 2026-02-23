#coding:utf-8

"""
ID:          issue-4144
ISSUE:       4144
TITLE:       Warnings could be put twice in status-vector
DESCRIPTION:
JIRA:        CORE-3801
FBTEST:      bugs.core_3801
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """
    set term ^ ;
    execute block as
        declare d date;
    begin
        d = 'now';
    end
    ^
"""

act = isql_act('db', test_script)

expected_stderr = """
    SQL warning code = 301
    -DATE data type is now called TIMESTAMP
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

