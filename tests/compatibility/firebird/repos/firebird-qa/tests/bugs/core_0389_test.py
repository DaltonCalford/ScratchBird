#coding:utf-8

"""
ID:          issue-732
ISSUE:       732
TITLE:       NULLS FIRST does not work with unions
DESCRIPTION:
JIRA:        CORE-389
FBTEST:      bugs.core_0389
"""

import pytest
from firebird.qa import *

init_script = """
    create table t(x int);
    insert into t values(2222);
    insert into t values(222 );
    insert into t values(22);
    insert into t values(2);
    insert into t values(null);
    insert into t values(null);
"""

db = db_factory(page_size=4096, init=init_script)

test_script = """
    select distinct x
    from t

    union all

    select distinct x
    from t

    order by 1 nulls first
    ;
    --------------------------
    select distinct x
    from t

    union all

    select distinct x
    from t

    order by 1 desc nulls first
    ;
    --------------------------
    select x
    from t

    union

    select x
    from t

    order by 1 nulls first
    ;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
               X
    ============
          <null>
          <null>
               2
               2
              22
              22
             222
             222
            2222
            2222


               X
    ============
          <null>
          <null>
            2222
            2222
             222
             222
              22
              22
               2
               2

               X
    ============
          <null>
               2
              22
             222
            2222
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

