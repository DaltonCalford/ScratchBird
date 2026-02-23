#coding:utf-8

"""
ID:          issue-5002
ISSUE:       5002
TITLE:       "Column unknown" error while preparing a recursive query if the recursive part contains ALIASED datasource in the join with anchor table
DESCRIPTION:
JIRA:        CORE-4694
FBTEST:      bugs.core_4694
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table xcall_stack (
        xcall_id int
        ,xcaller_id int
    );
    commit;

    with recursive
    r as (
        select c.xcall_id
        from xcall_stack c
        where c.xcaller_id is null

        UNION ALL

        select
               c.xcall_id
        from xcall_stack c
        join r
          AS h -- <<<<<<<<<<<<<<<<<<<<<< ::: NB ::: `r` is aliased
          on c.xcaller_id = h.xcall_id
    )
    select r.xcall_id
    from r;
"""

act = isql_act('db', test_script)

expected_stdout = """
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

