#coding:utf-8

"""
ID:          issue-4435
ISSUE:       4435
TITLE:       wrong resultset (subquery + derived table + union)
DESCRIPTION:
JIRA:        CORE-4107
FBTEST:      bugs.core_4107
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Fixed on FB 3.0 since 2015-07-12
    -- http://sourceforge.net/p/firebird/code/61970
    -- Checked on WI-V3.0.0.31940.
    set list on;
    select T.VAL1,
      (
        select 'something' from rdb$database where 2 = T.ID
        union
        select null from rdb$database where 1 = 0
      ) as VAL2
    from (
      select 1 as VAL1, 1 as ID from rdb$database
      union all
      select 2 as VAL1, 2 as ID from rdb$database
    ) as T
    group by 1;

    select T.VAL1,
      min((
        select 'something' from rdb$database where 2 = T.ID
        union
        select null from rdb$database where 1 = 0
      )) as VAL2
    from (
      select 1 as VAL1, 1 as ID from rdb$database
      union all
      select 2 as VAL1, 2 as ID from rdb$database
    ) as T
    group by 1;

"""

act = isql_act('db', test_script, substitutions=[('Statement failed.*', 'Statement failed')])

expected_stdout = """
    VAL1                            1
    VAL2                            <null>

    VAL1                            2
    VAL2                            something
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Invalid expression in the select list (not contained in either an aggregate function or the GROUP BY clause)
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

