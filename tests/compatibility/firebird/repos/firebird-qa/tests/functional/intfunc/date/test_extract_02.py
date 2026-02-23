#coding:utf-8

"""
ID:          intfunc.date.extract-02
ISSUE:       1805
TITLE:       EXTRACT - MILLISECONDS
DESCRIPTION: Test the extract function with miliseconds
JIRA:        CORE-1387
FBTEST:      functional.intfunc.date.extract_02
"""

import pytest
from firebird.qa import db_factory, isql_act, Action

db = db_factory()

test_script = """select extract(millisecond from time '12:12:00.1111' ) as test from rdb$database;
select extract(millisecond from timestamp '2008-12-08 12:12:00.1111' ) as test from rdb$database;"""

act = isql_act('db', test_script)

expected_stdout = """
        TEST
============
       111.1


        TEST
============
       111.1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
