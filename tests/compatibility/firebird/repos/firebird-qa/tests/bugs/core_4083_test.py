#coding:utf-8

"""
ID:          issue-4411
ISSUE:       4411
TITLE:       Full outer join in derived table with coalesce (iif)
DESCRIPTION:
JIRA:        CORE-4083
FBTEST:      bugs.core_4083
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
      A_SOME_FIELD,
      B_SOME_FIELD,
      C_SOME_FIELD,
      COALESCE_FIELD
    from
    (
      select
        A.SOME_FIELD as A_SOME_FIELD,
        B.SOME_FIELD as B_SOME_FIELD,
        C.SOME_FIELD as C_SOME_FIELD,
        coalesce(A.SOME_FIELD, B.SOME_FIELD, c.SOME_FIELD) as COALESCE_FIELD
      from
          (select null as SOME_FIELD from RDB$DATABASE) A
          full join
          (select 1 as SOME_FIELD from RDB$DATABASE) B on B.SOME_FIELD = A.SOME_FIELD
          full join
          (select null as SOME_FIELD from RDB$DATABASE) C on C.SOME_FIELD = B.SOME_FIELD
    ) x
    order by 1,2,3,4
    ;

    select a.*, b.*
    from
      (select 1 as field1 from rdb$database) a
      full join
      (select 2 as field2 from rdb$database) b on b.field2=a.field1
    order by 1,2
    ;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    A_SOME_FIELD                    <null>
    B_SOME_FIELD                    <null>
    C_SOME_FIELD                    <null>
    COALESCE_FIELD                  <null>
    A_SOME_FIELD                    <null>
    B_SOME_FIELD                    <null>
    C_SOME_FIELD                    <null>
    COALESCE_FIELD                  <null>
    A_SOME_FIELD                    <null>
    B_SOME_FIELD                    1
    C_SOME_FIELD                    <null>
    COALESCE_FIELD                  1

    FIELD1                          <null>
    FIELD2                          2
    FIELD1                          1
    FIELD2                          <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

