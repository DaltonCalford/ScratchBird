#coding:utf-8

"""
ID:          issue-2716
ISSUE:       2716
TITLE:       BUGCHECK 284
DESCRIPTION:
JIRA:        CORE-2291
FBTEST:      bugs.core_2291
"""

import pytest
from firebird.qa import *

init_script = """recreate table t (id int, f2 char(16));
commit;

insert into t values (1, '0123456798012345');
insert into t values (2, '0123456798012345');
commit;

alter table t drop f2;
commit;

update t set id = 3 where id = 2;
commit;
"""

db = db_factory(init=init_script)

test_script = """SET TERM !!;
execute block returns (id int)
as
begin
  select t1.id from t t1 left join t t2 on t1.id = t2.id - 2
   where t2.id = 3
    into :id;
  suspend;
end !!
"""

act = isql_act('db', test_script)

expected_stdout = """
          ID
============
           1

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

