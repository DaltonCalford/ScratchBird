#coding:utf-8

"""
ID:          issue-2480
ISSUE:       2480
TITLE:       Incorrect result with UPDATE OR INSERT ... RETURNING OLD and non-nullable columns
DESCRIPTION:
JIRA:        CORE-2044
FBTEST:      bugs.core_2044
"""

import pytest
from firebird.qa import *

init_script = """create table t (
    n integer primary key,
    x1 integer not null,
    x2 integer
);
"""

db = db_factory(init=init_script)

test_script = """update or insert into t
    values (1, 1, 1)
    returning old.n, old.x1, old.x2, new.n, new.x1, new.x2;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CONSTANT     CONSTANT     CONSTANT            N           X1           X2
============ ============ ============ ============ ============ ============
      <null>       <null>       <null>            1            1            1

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

