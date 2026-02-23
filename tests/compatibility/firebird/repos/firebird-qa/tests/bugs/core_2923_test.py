#coding:utf-8

"""
ID:          issue-3306
ISSUE:       3306
TITLE:       Problem with dependencies between a procedure and a view using that procedure
DESCRIPTION:
JIRA:        CORE-2923
FBTEST:      bugs.core_2923
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create procedure sp_test returns (i smallint) as
    begin
        i = 32767;
        suspend;
    end
    ^

    create view v0 as
    select i
    from sp_test
    ^

    alter procedure sp_test returns (i int) as
    begin
        i = 32768;
        suspend;
    end
    ^
    set term ;^
    commit;

    ---

    create table t1 (n1 smallint);

    insert into t1(n1) values(32767);
    commit;

    create view v1 as
    select *
    from t1;

    alter table t1 alter n1 type integer;
    commit;

    insert into t1(n1) values(32768);
    commit;

    ---

    create table t2 (n2 smallint);

    insert into t2(n2) values(32767);
    commit;

    create domain d2 integer;

    create view v2 as
    select * from t2;

    alter table t2 alter n2 type d2;

    insert into t2(n2) values(32768);
    commit;

    ---

    set list on;
    select '0' as test_no, v.* from v0 v
    union all
    select '1', v.* from v1 v
    union all
    select '2', v.* from v2 v
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    TEST_NO                         0
    I                               32768
    TEST_NO                         1
    I                               32767
    TEST_NO                         1
    I                               32768
    TEST_NO                         2
    I                               32767
    TEST_NO                         2
    I                               32768
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

