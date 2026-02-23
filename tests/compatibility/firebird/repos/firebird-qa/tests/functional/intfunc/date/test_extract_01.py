#coding:utf-8

"""
ID:          intfunc.date.extract-01
ISSUE:       1029
TITLE:       EXTRACT(WEEK FROM DATE)
DESCRIPTION: Test the extract week function
JIRA:        CORE-663
FBTEST:      functional.intfunc.date.extract_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select extract(week from date '30.12.2008'), extract(week from date '30.12.2009') from rdb$database;")

expected_stdout = """
EXTRACT EXTRACT
======= =======
      1      53
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
