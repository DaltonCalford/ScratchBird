#coding:utf-8

"""
ID:          issue-4150
ISSUE:       4150
TITLE:       Error "Invalid expression in the select list" can be unexpectedly raised if
  a string literal is used inside a GROUP BY clause in a multi-byte connection
DESCRIPTION:
JIRA:        CORE-3807
FBTEST:      bugs.core_3807
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """select
    'Current time is ' || cast(a.t as varchar(15))
from
    (select '16:06:03.0000' t from rdb$database) a
group by
    'Current time is ' || cast(a.t as varchar(15));
"""

act = isql_act('db', test_script)

expected_stdout = """
CONCATENATION
===============================
Current time is 16:06:03.0000

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

