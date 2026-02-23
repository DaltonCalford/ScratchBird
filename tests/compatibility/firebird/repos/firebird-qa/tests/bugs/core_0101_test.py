#coding:utf-8

"""
ID:          issue-425
ISSUE:       425
TITLE:       JOIN the same table - problem with alias names
DESCRIPTION:
JIRA:        CORE-101
FBTEST:      bugs.core_0101
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Confirmed wrong result on WI-V1.5.6.5026, all above wirks fine.
    recreate table test(id int);
    insert into test values(1);
    insert into test values(-1);
    insert into test values(-2);
    insert into test values(2);
    commit;

    --set plan on;
    set list on;

    select *
    from (
        select test.id a_id, b.id as b_id
        from test test
        join test b on test.id = b.id
    ) order by 1,2
    ;

    create index test_id on test(id);

    select *
    from (
        select test.id a_id, b.id as b_id
        from test test
        join test b on test.id = b.id
    ) order by 1,2
    ;

    select *
    from (
        select test.id a_id, b.id as b_id
        from (select id from test order by id) test
        join (select id from test order by id) b on test.id = b.id
    ) order by 1,2
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    A_ID                            -2
    B_ID                            -2

    A_ID                            -1
    B_ID                            -1

    A_ID                            1
    B_ID                            1

    A_ID                            2
    B_ID                            2



    A_ID                            -2
    B_ID                            -2

    A_ID                            -1
    B_ID                            -1

    A_ID                            1
    B_ID                            1

    A_ID                            2
    B_ID                            2



    A_ID                            -2
    B_ID                            -2

    A_ID                            -1
    B_ID                            -1

    A_ID                            1
    B_ID                            1

    A_ID                            2
    B_ID                            2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

