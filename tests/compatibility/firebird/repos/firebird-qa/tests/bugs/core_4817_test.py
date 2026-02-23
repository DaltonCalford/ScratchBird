#coding:utf-8

"""
ID:          issue-1927
ISSUE:       1927
TITLE:       ISQL doesn`t show number of affected rows for "MERGE ... WHEN MATCHING" in
  case when this number surely > 0
DESCRIPTION:
JIRA:        CORE-4817
FBTEST:      bugs.core_4817
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table src(id int, x int);
    commit;

    insert into src
    with recursive r as(select 0 i from rdb$database union all select r.i+1 from r where r.i<9)
    select r.i, r.i * 2 from r;
    commit;

    recreate table tgt(id int primary key, x int);
    commit;

    set count on;
    merge into tgt t
    using (select * from src where id > 1) s on s.id = t.id
    when not matched then insert values(s.id, s.x);
    commit;

    merge into tgt t
    using src s on s.id = t.id
    when matched and s.id in (7,8,9) then update set t.x = -2 * s.x;
    rollback;

    merge into tgt t
    using src s on s.id = t.id
    when matched and s.id in (0,1,2) then update set t.x = -2 * s.x
    when not matched and s.id = 0 then insert values(s.id, 1111)
    when matched and s.id in (3,4,5,6) then update set t.x = - 3 * s.x
    when not matched and s.id = 1 then insert values(s.id, 2222)
    when matched then delete
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
   Records affected: 8
   Records affected: 3
   Records affected: 10
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

