#coding:utf-8

"""
ID:          issue-749
ISSUE:       749
TITLE:       Garbage vs indices/constraints
DESCRIPTION:
  Confirmed bug on 3.0.4.32924, got:
    DatabaseError:
    Error while commiting transaction:
    - SQLCODE: -803
    - attempt to store duplicate value (visible to active transactions) in unique index "TEST_X"
    -803
    335544349
JIRA:        CORE-405
FBTEST:      bugs.core_0405
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """2
2
3
3
0
TEST_X
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        con.execute_immediate('recreate table test(x int)')
        con.commit()
        cur = con.cursor()
        cur.executemany('insert into test( x ) values( ? )', [(2,), (3,), (3,), (2,)])
        for r in cur.execute('select x from test order by x'):
            print(r[0])
        cur.execute('delete from test')
        for r in cur.execute('select count(*) from test'):
            print(r[0])
        con.execute_immediate('create unique index test_x on test(x)')
        con.commit()
        for r in cur.execute("select rdb$index_name from rdb$indices where rdb$relation_name='TEST'"):
            print(r[0].rstrip())
    output = capsys.readouterr()
    assert output.out == expected_stdout


