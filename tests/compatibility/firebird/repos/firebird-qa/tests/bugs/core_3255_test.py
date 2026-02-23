#coding:utf-8

"""
ID:          issue-3623
ISSUE:       3623
TITLE:       The server could crash using views with GROUP BY
DESCRIPTION:
JIRA:        CORE-3255
FBTEST:      bugs.core_3255
"""

import pytest
from firebird.qa import *

init_script = """SET TERM !;
create table t1 (
  n1 integer
)!

create view v1 (x, n1) as
  select 'a', n1
    from t1
group by 1, n1!

insert into t1 values (1)!
insert into t1 values (1)!
insert into t1 values (2)!
insert into t1 values (2)!
insert into t1 values (3)!

commit!

create or alter procedure p1 returns (x varchar(1), n1 integer)
as
begin
  for select x, n1 from v1 into x, n1 do
      suspend;
end!

create or alter procedure p2 returns (x varchar(1), n1 integer)
as
begin
  for select n1 from t1 into n1 do
  begin
  end

  for select n1, x from v1 into n1, x do
      suspend;
end!

commit!
SET TERM ;!
"""

db = db_factory(init=init_script)

test_script = """select * from p2;
select * from p1;
"""

act = isql_act('db', test_script)

expected_stdout = """
X                N1
====== ============
a                 1
a                 2
a                 3

X                N1
====== ============
a                 1
a                 2
a                 3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

