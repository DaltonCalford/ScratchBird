#coding:utf-8

"""
ID:          issue-1577
ISSUE:       1577
TITLE:       Prepare fails when having a parameter in a DSQL statement before a sub query
DESCRIPTION:
NOTES:
[19.1.2022]
  Using WITH for prepare() call is important to dispose returned prepared statement before
  connection is closed. Otherwise pytest will report unhandled exceptions in __del__ calls
  as prepared statement objects are destructed at wrong time.
JIRA:        CORE-1156
FBTEST:      bugs.core_1156
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        try:
            with c.prepare('select count(*) from rdb$database where ? < (select count(*) from rdb$database)'):
                pass
        except:
            pytest.fail('Test FAILED in case 1')
        try:
            with c.prepare('select count(*) from rdb$database where (select count(*) from rdb$database) > ?'):
                pass
        except:
            pytest.fail('Test FAILED in case 2')
        try:
            with c.prepare('select count(*) from rdb$database where ? < cast ((select count(*) from rdb$database) as integer)'):
                pass
        except:
            pytest.fail('Test FAILED in case 3')
        try:
            with c.prepare('select count(*) from rdb$database where 0 < (select count(*) from rdb$database)'):
                pass
        except:
            pytest.fail('Test FAILED in case 4')
        try:
            with c.prepare('select count(*) from rdb$database where cast (? as integer) < (select count(*) from rdb$database)'):
                pass
        except:
            pytest.fail('Test FAILED in case 5')
