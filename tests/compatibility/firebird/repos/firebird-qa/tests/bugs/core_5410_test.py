#coding:utf-8

"""
ID:          issue-5683
ISSUE:       5683
TITLE:       Dependencies are not stored when using some type of contructions in subroutines
DESCRIPTION:
JIRA:        CORE-5410
FBTEST:      bugs.core_5410
NOTES:
    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create or alter procedure sp_outer_standalone as begin end;
    create or alter package pg_test as begin end;
    commit;

    recreate table test_1(f01 integer);
    insert into test_1(f01) values(100);
    insert into test_1(f01) values(1);
    insert into test_1(f01) values(10);
    commit;

    recreate table test_2(f02 integer);
    commit;
    insert into test_2(f02) values(200);
    insert into test_2(f02) values(2);
    insert into test_2(f02) values(20);
    commit;

    recreate table test_3(f02 integer);
    commit;
    insert into test_3(f02) values(300);
    insert into test_3(f02) values(3);
    insert into test_3(f02) values(30);
    commit;

    set term ^;
    create or alter procedure sp_outer_standalone returns(max_f01 int) as
        declare procedure sp_inner returns(max_f01 int) as
        begin
            select max(f01) from test_1 into max_f01;
            suspend;
        end
    begin
        select max_f01 from sp_inner into max_f01;
        suspend;
    end;
    ^

    create or alter function fn_outer_standalone returns int as
        declare function fn_inner returns int as
        begin
            return (select min(f01) from test_1);
        end
    begin
        return fn_inner();
    end;
    ^

    create or alter package pg_test as
    begin
        procedure sp_outer_packaged returns(max_f02 int);
        function fn_outer_packaged returns int;
    end
    ^
    recreate package body pg_test as
    begin
        procedure sp_outer_packaged returns(max_f02 int) as
            declare procedure sp_inner returns(max_f02 int) as
            begin
                select max(f02) from test_2 into max_f02;
                suspend;
            end
        begin
            select max_f02 from sp_inner into max_f02;
            suspend;
        end

        function fn_outer_packaged returns int as
            declare function fn_inner returns int as
            begin
                return (select min(f02) from test_2);
            end
        begin
            return fn_inner();
        end
    end
    ^

    set term ;^
    commit;

    -----------------------

    drop table test_1; -- should FAIL!
    drop table test_2; -- should FAIL!
    commit;

    select max_f01 from sp_outer_standalone;
    select fn_outer_standalone() as min_f01 from rdb$database;

    select max_f02 from pg_test.sp_outer_packaged;
    select pg_test.fn_outer_packaged() as min_f02 from rdb$database;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN TEST_1.F01
    -there are 2 dependencies

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN TEST_2.F02
    -there are 1 dependencies

    MAX_F01 100
    MIN_F01 1
    MAX_F02 200
    MIN_F02 2
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN "PUBLIC"."TEST_1"."F01"
    -there are 2 dependencies

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN "PUBLIC"."TEST_2"."F02"
    -there are 1 dependencies

    MAX_F01 100
    MIN_F01 1
    MAX_F02 200
    MIN_F02 2
"""

@pytest.mark.version('>=3.0.2')
def test_1(act: Action):

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
