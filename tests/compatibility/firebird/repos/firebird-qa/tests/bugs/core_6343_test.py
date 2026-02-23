#coding:utf-8

"""
ID:          issue-6584
ISSUE:       6584
TITLE:       Rolled back transaction produces unexpected results leading to duplicate values in PRIMARY KEY field
DESCRIPTION:
JIRA:        CORE-6343
FBTEST:      bugs.core_6343
notes:
    [03.07.2025] pzotov
    Suppress name of stored procedure from output - it has no matter in this test.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure sp_test as begin end;

    create global temporary table gtt_table (
        id integer not null
    ) on commit delete rows;

    create table test (
        id integer not null primary key
    );
    set term ^;
    create or alter procedure sp_test returns ( id1 integer)
    as
       declare variable v integer;
    begin
       insert into gtt_table values(1);
       insert into gtt_table values(2);
       insert into gtt_table values(3);

       for
           select id from gtt_table
           into :id1 do
       begin
             insert into test (id) values (:id1);

             -- NOTE: it is mandatory to make "unnecessary" query to rdb$database
             -- in order to reproduce bug. Do not replace it with "pure PSQL".
             for
                 select 1 from rdb$database into :v
             do
                if (:id1=3) then
                    id1 = 1/0; -- do NOT suppress this exception otherwise bug will not shown

             suspend;

             delete from test;
        end
    end
    ^
    set term ;^
    commit;
    -----------------------------------
    set heading off;
    -- Iteration #1
    select * from sp_test;
    rollback;
    select * from test;


    -- Iteration #2
    select * from sp_test;
    rollback;
    select * from test;


    -- Iteration #3
    select * from sp_test;
    rollback;
    select * from test;


    -- Iteration #4
    select * from sp_test;
    rollback;
    select * from test;

"""

act = isql_act('db', test_script, substitutions=[('line:.*', ''), ('(-)?At procedure .*', '')])

expected_stdout = """
    1
    2
    Statement failed, SQLSTATE = 22012
    arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
    1
    2
    Statement failed, SQLSTATE = 22012
    arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
    1
    2
    Statement failed, SQLSTATE = 22012
    arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
    1
    2
    Statement failed, SQLSTATE = 22012
    arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
"""

@pytest.mark.version('>=3.0.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
