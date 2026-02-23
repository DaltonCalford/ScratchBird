#coding:utf-8

"""
ID:          issue-3277
ISSUE:       3277
TITLE:       Expression in a subquery may be treated as invariant and produce incorrect results
DESCRIPTION:
JIRA:        CORE-2893
FBTEST:      bugs.core_2893
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test_z (c varchar(10));
    commit;

    insert into test_z values (1);
    insert into test_z values (1);
    insert into test_z values (2);
    insert into test_z values (3);
    commit;

    -- From CORE-3031:
    create view v_test (f1, f2, f3) as
    select '1.1', '1.2', '1.3' from rdb$database
    union all
    select '2.1', '2.2', '2.3' from rdb$database
    union all
    select '3.1', '3.2', '3.3' from rdb$database
    ;
"""

db = db_factory(init=init_script)

test_script = """
    set count on;
    set list on;

    select 'Test-1' as msg, t.*
    from (
      select (select case when R.RDB$Relation_ID = 0 then 0 else 1 end from RDB$Database) TypeID
      from RDB$Relations R
      where R.RDB$Relation_ID < 2
    ) t;

    select 'Test-2' as msg, z.c
    from test_z z
    where
        (
          select z.c || '' from rdb$database
        ) = '1'
    ;
    commit;

    -- From CORE-3031:
    select 'Test-3' as msg, t.*
    from (
      select
          t.f1 || '; ' || t.f2 || '; ' || t.f3 as f123_concat,
          (
            select
              '' || t.f3
            from rdb$database
          ) as f3_concat
      from v_test t
    ) t;
"""

act = isql_act('db', test_script)

expected_stdout = """
    MSG                             Test-1
    TYPEID                          0
    MSG                             Test-1
    TYPEID                          1
    Records affected: 2
    MSG                             Test-2
    C                               1
    MSG                             Test-2
    C                               1
    Records affected: 2
    MSG                             Test-3
    F123_CONCAT                     1.1; 1.2; 1.3
    F3_CONCAT                       1.3
    MSG                             Test-3
    F123_CONCAT                     2.1; 2.2; 2.3
    F3_CONCAT                       2.3
    MSG                             Test-3
    F123_CONCAT                     3.1; 3.2; 3.3
    F3_CONCAT                       3.3
    Records affected: 3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

