#coding:utf-8

"""
ID:          issue-1416
ISSUE:       1416
TITLE:       DISTINCT vs NULLS LAST clause: wrong order of NULLs
DESCRIPTION:
JIRA:        CORE-1005
FBTEST:      bugs.core_1005
"""

import pytest
from firebird.qa import *

init_script = """create table T (A int, B int) ;
commit ;

insert into T values (1,1);
insert into T values (1,1);
insert into T values (2,2);
insert into T values (3,3);
insert into T values (null,null);
insert into T values (null,null);
insert into T values (4,4);
commit ;
"""

db = db_factory(init=init_script)

test_script = """select distinct A, B from T order by A nulls last, B nulls last ;
select distinct A, B from T order by A nulls last ;
select distinct A, B from T order by B nulls last ;
"""

act = isql_act('db', test_script)

expected_stdout = """A            B
============ ============
           1            1
           2            2
           3            3
           4            4
      <null>       <null>

A            B
============ ============
           1            1
           2            2
           3            3
           4            4
      <null>       <null>

A            B
============ ============
           1            1
           2            2
           3            3
           4            4
      <null>       <null>

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

