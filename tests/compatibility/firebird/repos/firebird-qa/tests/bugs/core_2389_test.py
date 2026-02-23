#coding:utf-8

"""
ID:          issue-2811
ISSUE:       2811
TITLE:       Wrong matching of SIMILAR TO expression with brackets
DESCRIPTION:
JIRA:        CORE-2389
FBTEST:      bugs.core_2389
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select 1 from rdb$database where 'x/t' SIMILAR TO '%[/]t';
"""

act = isql_act('db', test_script)

expected_stdout = """
    CONSTANT
============
           1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

