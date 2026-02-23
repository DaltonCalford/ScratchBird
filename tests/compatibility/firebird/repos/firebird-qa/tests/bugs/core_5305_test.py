#coding:utf-8

"""
ID:          issue-5582
ISSUE:       5582
TITLE:       CASCADE UPDATE fails for self-referencing FK
DESCRIPTION:
JIRA:        CORE-5305
FBTEST:      bugs.core_5305
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test1(
        x int,
        y int,
        constraint test1_pk primary key(x),
        constraint test1_fk foreign key(y) references test1(x) on update cascade
    );
    commit;

    insert into test1 (x, y) values (1, null);
    insert into test1 (x, y) values (2, null);
    insert into test1 (x, y) values (3, 1);
    commit;

    -- 4.0.0.322 - OK
    -- 3.0.0.32483:
    -- Statement failed, SQLSTATE = 23000
    -- violation of FOREIGN KEY constraint "test1_FK" on table "test1"
    -- -Foreign key reference target does not exist
    -- -Problematic key value is ("y" = 1)

    update test1 set x = -x;
    select 'test1' as what, a.* from test1 a order by x;
    commit;

    --------------------------------------------------------------------------------------

    -- Sample from core-3362, 03/Jan/14 05:07 PM:
    -- ==========================================
    recreate table test2(
        x int, y int,
        constraint t_pk primary key(x),
        constraint t_fk foreign key(y) references test2(x) on update cascade -- SELF-REFERENCING
    );
    create descending index t_x_desc on test2(x);
    commit;

    insert into test2(x, y) values(1, null);
    insert into test2(x, y) values(2, 1 );
    insert into test2(x, y) values(3, 2 );
    insert into test2(x, y) values(4, 3 );
    insert into test2(x, y) values(5, 4 );
    update test2 set y=5 where x=1; -- "closure" of chain
    commit;

    update test2 set x = y + 1;

    select 'test2' as what,  a.* from test2 a order by x;
    commit;

    ----------------------------------------------------------------------------

    -- Sample from core-3362, 14/May/15 06:40 AM
    -- =========================================

    recreate table test3(
        id int constraint test_pk_id primary key using index test_pk_id,
        pid int constraint test_fk_pid2id references test3(id)
        on update SET NULL
    );
    commit;
    insert into test3 values( 5, null );
    insert into test3 values( 4, 5 );
    insert into test3 values( 3, 4 );
    insert into test3 values( 2, 3 );
    insert into test3 values( 1, 2 );
    update test3 set pid=1 where id=5; --"closure" of chain
    commit;

    update  test3 set id = id + 1 order by id desc;
    select 'test3' as what, a.* from test3 a order by id;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    WHAT                            test1
    X                               -3
    Y                               -1

    WHAT                            test1
    X                               -2
    Y                               <null>

    WHAT                            test1
    X                               -1
    Y                               <null>



    WHAT                            test2
    X                               2
    Y                               6

    WHAT                            test2
    X                               3
    Y                               2

    WHAT                            test2
    X                               4
    Y                               3

    WHAT                            test2
    X                               5
    Y                               4

    WHAT                            test2
    X                               6
    Y                               5



    WHAT                            test3
    ID                              2
    PID                             <null>

    WHAT                            test3
    ID                              3
    PID                             <null>

    WHAT                            test3
    ID                              4
    PID                             <null>

    WHAT                            test3
    ID                              5
    PID                             <null>

    WHAT                            test3
    ID                              6
    PID                             <null>
"""

@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

