#coding:utf-8

"""
ID:          issue-4594
ISSUE:       4594
TITLE:       Error in case of subquery with windowed function + where <field> IN(select ...)
DESCRIPTION:
JIRA:        CORE-4270
FBTEST:      bugs.core_4270
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table t0(q int); commit;
    create index t0_q on t0(q);
    commit;
    insert into t0(q) values (1);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select a.qx
    from
    (
     select qx
     from
     (
         select 1 qx
               ,count(*)over() as c
         from t0
         where t0.q in (select 1 from rdb$database)
     ) r
    ) a
    join t0 b on a.qx = b.q;
"""

act = isql_act('db', test_script)

expected_stdout = """
  QX                              1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

