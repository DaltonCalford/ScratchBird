#coding:utf-8

"""
ID:          issue-2549
ISSUE:       2549
TITLE:       Incorrect ROW_COUNT value with indexed retrieval and subquery
DESCRIPTION:
JIRA:        CORE-2117
FBTEST:      bugs.core_2117
"""

import pytest
from firebird.qa import *

init_script = """create table t (n integer);

insert into t values (1);
insert into t values (2);
insert into t values (3);

commit;

create index t_n on t (n);
commit;
"""

db = db_factory(init=init_script)

test_script = """set term !!;

execute block returns (n integer)
as
  declare x integer;
begin
  update t set n = n where n = -1;
  n = row_count;
  suspend;

  update t set n = n where n = (select -1 from rdb$database);
  n = row_count;
  suspend;

  update t set n = n where n = -1;
  n = row_count;
  suspend;

  for select 1 from t where n = -1 into x do
  begin
  end
  n = row_count;
  suspend;

  for select 1 from t where n = (select -1 from rdb$database) into x do
  begin
  end
  n = row_count;
  suspend;

  delete from t where n = (select -1 from rdb$database);
  n = row_count;
  suspend;
end!!

set term ;!!"""

act = isql_act('db', test_script)

expected_stdout = """
           N
============
           0
           0
           0
           0
           0
           0

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

