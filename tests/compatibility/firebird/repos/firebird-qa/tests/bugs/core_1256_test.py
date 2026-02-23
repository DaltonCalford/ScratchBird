#coding:utf-8

"""
ID:          issue-703
ISSUE:       703
TITLE:       Table columns hide destination variables of RETURNING INTO
DESCRIPTION:
JIRA:        CORE-1256
FBTEST:      bugs.core_1256
"""

import pytest
from firebird.qa import *

init_script = """create table t (n integer) ;
"""

db = db_factory(init=init_script)

test_script = """set term ^;

-- ok

execute block returns (n integer)
as
begin
  insert into t values (1) returning n into :n;
  suspend;
end^

-- not ok

execute block returns (n integer)
as
begin
  insert into t values (1) returning n into n;
  suspend;
end^

set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
           N
============
           1


           N
============
           1

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

