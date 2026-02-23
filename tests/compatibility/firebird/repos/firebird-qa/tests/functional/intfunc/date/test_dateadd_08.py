#coding:utf-8

"""
ID:          intfunc.date.dateadd-08
ISSUE:       1805
TITLE:       Dateadd milliseconds
DESCRIPTION:
JIRA:        CORE-1387
FBTEST:      functional.intfunc.date.dateadd_08
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select dateadd(-1 millisecond to time '12:12:00:0000' ) as test from rdb$database;
select dateadd(millisecond,-1, time '12:12:00:0000' ) as test from rdb$database;"""

act = isql_act('db', test_script)

expected_stdout = """
         TEST
=============
12:11:59.9990


         TEST
=============
12:11:59.9990
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
