#coding:utf-8

"""
ID:          issue-1568
ISSUE:       1568
TITLE:       AV in rse\\invalidate_child_rpbs for recursive queies
DESCRIPTION:
JIRA:        CORE-1146
FBTEST:      bugs.core_1146
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table c (
      id integer,
      name varchar(100)
    );
    recreate table t (
      id integer,
      ownercode integer,
      code integer
    );
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    -- Updated code to recent FB version: add prefixes to every field from the query
    with recursive x as
    (
      select --1 as step,
             t.id, t.ownercode, t.code, c2.name as ownclass, c1.name as class
        from t inner join c c1 on c1.id = t.code
               left join c c2 on c2.id = t.ownercode
        where ownercode = 0

      union all

      select --x.step+1
             t.id, t.ownercode, x.code, c2.name as ownclass, c1.name as class
        from t inner join c c1 on c1.id = t.code
               left join c c2 on c2.id = t.ownercode
               inner join x on t.ownercode = x.code
    )
    select * from x
    ;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
