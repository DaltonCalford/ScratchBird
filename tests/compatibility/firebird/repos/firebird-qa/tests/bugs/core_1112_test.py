#coding:utf-8

"""
ID:          issue-1532
ISSUE:       1532
TITLE:       Crash when dealing with a string literal longer than 32K
DESCRIPTION:
JIRA:        CORE-1112
FBTEST:      bugs.core_1112
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        longstr = 'abc' * 10930
        c.execute(f"select * from rdb$database where '{longstr}' = 'a'")
        c.execute(f"select * from rdb$database where '{longstr}' containing 'a'")
        c.execute("select 'a' from rdb$database")
        result = c.fetchall()
        assert result == [('a',)]


