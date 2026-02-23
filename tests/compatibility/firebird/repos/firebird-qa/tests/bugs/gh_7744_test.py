#coding:utf-8

"""
ID:          issue-7744
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7744
TITLE:       Provide ability to run "ALTER SQL SECURITY DEFINER / INVOKER" without specifying further part of proc/func/package
DESCRIPTION:
    We create two users: 'senior' and 'junior'.
    user 'senior' is granted with DDL privileges to create and alter table, function, procedure and package.
    He ('senior') creates table and three PSQL units that uses it (func/proc /package), and give grant to execute this PSQL units to 'junior'.
    NOTE-1. User 'junior' has NO access to the table 'TEST', so 'permission error' would raise if we dont specify SQL SECURITY in PSQL units.
    NOTE-2. We intentionally specify 'SQL SECURITY DEFINER', thus 'junior' initially must be ABLE to call them without permission error.
    Then we replace SQL SECURITY attribute for all PSQL units with 'INVOKER' and repeat attempt to call them by 'junior' (it must FAIL).
    After this we return SQL SECURITY attribute to 'DEFINER' and repeat the same. This attempt must complete with success.
    Finally, we DROP SQL SECURITY. This must again cause permission error for call of every PSQL units.
NOTES:
    [05.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

u_senior = user_factory('db', name='tmp$7744_senior', password='123')
u_junior = user_factory('db', name='tmp$7744_junior', password='456')

@pytest.mark.version('>=6.0')
def test_1(act: Action, u_senior: User, u_junior: User):

    test_script = f"""
        set list on;
        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

        grant create table to {u_senior.name};
        grant create procedure to {u_senior.name};
        grant create function to {u_senior.name};
        grant create package to {u_senior.name};
        grant alter any table to {u_senior.name};
        grant alter any procedure to {u_senior.name};
        grant alter any function to {u_senior.name};
        grant alter any package to {u_senior.name};
        commit;

        connect '{act.db.dsn}' user {u_senior.name} password '{u_senior.password}';

        create table test(id int primary key, x int);
        insert into test(id, x) values( 1,  100);
        insert into test(id, x) values( 2,  200);
        insert into test(id, x) values( 3,  300);
        insert into test(id, x) values( 4,  400);
        insert into test(id, x) values( 5,  500);
        insert into test(id, x) values( 6,  600);
        insert into test(id, x) values( 7,  700);
        insert into test(id, x) values( 8,  800);
        insert into test(id, x) values( 9,  900);
        insert into test(id, x) values(10, 1000);
        insert into test(id, x) values(11, 1100);
        insert into test(id, x) values(12, 1200);
        commit;

        set term ^;
        create function fn_cube_root(a_id int) returns decfloat SQL SECURITY DEFINER as
        begin
            return power( (select x from test where id = :a_id), 1/cast(3. as decfloat) );
        end
        ^
        create procedure sp_cube_root(a_id int) returns(cube_root decfloat) SQL SECURITY DEFINER as
        begin
            cube_root = power( (select x from test where id = :a_id), 1/cast(3. as decfloat) );
            suspend;
        end
        ^
        commit
        ^
        create package pg_test SQL SECURITY DEFINER as
        begin
            function fn_cube_root(a_id int) returns decfloat;
        end
        ^
        create package body pg_test as
        begin
            function fn_cube_root(a_id int) returns decfloat as
            begin
                return power( (select x from test where id = :a_id), 1/cast(3. as decfloat) );
            end
        end
        ^
        set term ;^
        commit;

        grant execute on function fn_cube_root to {u_junior.name};
        grant execute on procedure sp_cube_root to {u_junior.name};
        grant execute on package pg_test to {u_junior.name};
        commit;

        set bail off;

        -------------------------------------------

        connect '{act.db.dsn}' user {u_junior.name} password '{u_junior.password}';

        -- all these calls must PASS despite that there is no permission to the table 'test' 
        -- because sql security was initially set to 'DEFINER':
        select fn_cube_root(1) as res_1 from rdb$database;
        select cube_root as res_2 from sp_cube_root(2);
        select pg_test.fn_cube_root(3) as res_3 from rdb$database;
        rollback;

        -------------------------------------------

        connect '{act.db.dsn}' user {u_senior.name} password '{u_senior.password}';
        alter function fn_cube_root SQL SECURITY INVOKER;
        alter procedure sp_cube_root SQL SECURITY INVOKER;
        alter package pg_test SQL SECURITY INVOKER;
        commit;

        -- now these calls must FAIL with 'no permission ... to TEST':
        connect '{act.db.dsn}' user {u_junior.name} password '{u_junior.password}';
        select fn_cube_root(4) as res_4 from rdb$database;
        select cube_root as res_5 from sp_cube_root(5);
        select pg_test.fn_cube_root(6) as res_6 from rdb$database;
        rollback;

        -------------------------------------------

        connect '{act.db.dsn}' user {u_senior.name} password '{u_senior.password}';
        alter function fn_cube_root SQL SECURITY DEFINER;
        alter procedure sp_cube_root SQL SECURITY DEFINER;
        alter package pg_test SQL SECURITY DEFINER;
        commit;

        connect '{act.db.dsn}' user {u_junior.name} password '{u_junior.password}';

        -- now these calls must again PASS:
        select fn_cube_root(7) as res_7 from rdb$database;
        select cube_root as res_8 from sp_cube_root(8);
        select pg_test.fn_cube_root(9) as res_9 from rdb$database;
        rollback;

        -------------------------------------------

        connect '{act.db.dsn}' user {u_senior.name} password '{u_senior.password}';
        alter function fn_cube_root DROP SQL SECURITY;
        alter procedure sp_cube_root DROP SQL SECURITY;
        alter package pg_test DROP SQL SECURITY;
        commit;

        connect '{act.db.dsn}' user {u_junior.name} password '{u_junior.password}';

        -- now these calls must FAIL with 'no permission ... to TEST':
        select fn_cube_root(10) as res_10 from rdb$database;
        select cube_root as res_11 from sp_cube_root(11);
        select pg_test.fn_cube_root(12) as res_12 from rdb$database;
        rollback;
    """

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TABLE_TEST_NAME = 'TEST' if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TEST"'

    expected_stdout = f"""
        RES_1                                  4.641588833612778892410076350919446
        RES_2                                  5.848035476425732131013574720275845
        RES_3                                  6.694329500821695218826593246399307

        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE {TABLE_TEST_NAME}
        -Effective user is {u_junior.name}

        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE {TABLE_TEST_NAME}
        -Effective user is {u_junior.name}

        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE {TABLE_TEST_NAME}
        -Effective user is {u_junior.name}

        RES_7                                  8.879040017426007084292689552528769
        RES_8                                  9.283177667225557784820152701838891
        RES_9                                  9.654893846056297578599327844350667

        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE {TABLE_TEST_NAME}
        -Effective user is {u_junior.name}

        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE {TABLE_TEST_NAME}
        -Effective user is {u_junior.name}

        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE {TABLE_TEST_NAME}
        -Effective user is {u_junior.name}
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_script, connect_db = False, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
