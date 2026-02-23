#coding:utf-8

"""
ID:          issue-6942
ISSUE:       6942
TITLE:       Incorrect singleton error with MERGE and RETURNING
DESCRIPTION:
FBTEST:      bugs.gh_6942
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(
        n1 integer,
        n2 integer
    );

    insert into test values (1, 10);
    insert into test values (2, 20);
    commit;

    --------------------------------------------------

    merge into test
        using (
            select 2 x from rdb$database
            union all
            select 3 x from rdb$database
        ) t
            on test.n1 = t.x
        when not matched then insert values (3, 30)
        returning n1, n2;
    rollback;

    --------------------------------------------------

    set term ^;
    execute block returns (
        o1 integer,
        o2 integer
    )
    as
    begin
        merge into test
            using (
                select 2 x from rdb$database
                union all
                select 3 x from rdb$database
            ) t
                on test.n1 = t.x
            when not matched then insert values (3, 30)
            returning n1, n2 into o1, o2;

        suspend;
    end
    ^
    set term ;^

"""

act = isql_act('db', test_script)

expected_stdout = """
    N1                              3
    N2                              30
    O1                              3
    O2                              30
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
