#coding:utf-8

"""
ID:          issue-3614
ISSUE:       3614
TITLE:       POSITION: Wrong result with '' if third argument present
DESCRIPTION:
JIRA:        CORE-3244
FBTEST:      bugs.core_3244
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select position ('', 'Broehaha') from rdb$database;
select position ('', 'Broehaha', 4) from rdb$database;
select position ('', 'Broehaha', 20) from rdb$database;"""

act = isql_act('db', test_script)

expected_stdout = """
    POSITION
============
           1

    POSITION
============
           4

    POSITION
============
           0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

