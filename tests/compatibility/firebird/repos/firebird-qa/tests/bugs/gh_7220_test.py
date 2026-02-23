#coding:utf-8

"""
ID:          issue-7220
ISSUE:       7220
TITLE:       Dependencies of subroutines are not preserved after backup restore
DESCRIPTION:
NOTES:
    [23.02.2023] pzotov
        Confirmed bug on 5.0.0.520 (but 'drop table' will not fail only if it is executed in the same connect as DDL).
        Checked on 5.0.0.958 - all fine.
    [04.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.863; 5.0.3.1668.
"""
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

test_sql = """
    set term ^;
    create domain domain1 integer
    ^
    create table table1 (n integer, c varchar(31))
    ^
    create package pkg as
    begin
        procedure proc1(i type of column table1.n);
        procedure proc2(i domain1);
    end
    ^
    create function wait_event (event_name type of column table1.c)
        returns integer not null
        external name 'udrcpp_example!wait_event' engine udr
    ^
    set term ;^
    commit;
    
    set list on;
    set count on;
    select
        rdb$dependent_name
        ,rdb$depended_on_name
    from rdb$dependencies
    order by rdb$dependent_name;

    -- NB: NO re-connect must be here!
    -- Otherwise 'drop table' *will* fail on build where this bug was found.

    set echo on;

    drop table table1;
    
    drop domain domain1;
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):

    expected_stdout_5x = """
        RDB$DEPENDENT_NAME              PKG
        RDB$DEPENDED_ON_NAME            TABLE1

        RDB$DEPENDENT_NAME              PKG
        RDB$DEPENDED_ON_NAME            DOMAIN1

        RDB$DEPENDENT_NAME              WAIT_EVENT
        RDB$DEPENDED_ON_NAME            TABLE1
        Records affected: 3

        drop table table1;
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN TABLE1.N
        -there are 1 dependencies

        drop domain domain1;
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP DOMAIN DOMAIN1 failed
        -Domain DOMAIN1 is used in procedure PKG.PROC2 (parameter name I) and cannot be dropped
    """

    expected_stdout_6x = """
        RDB$DEPENDENT_NAME              PKG
        RDB$DEPENDED_ON_NAME            TABLE1
        RDB$DEPENDENT_NAME              PKG
        RDB$DEPENDED_ON_NAME            DOMAIN1
        RDB$DEPENDENT_NAME              WAIT_EVENT
        RDB$DEPENDED_ON_NAME            TABLE1
        Records affected: 3
        drop table table1;
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -cannot delete
        -COLUMN "PUBLIC"."TABLE1"."N"
        -there are 1 dependencies
        drop domain domain1;
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP DOMAIN "PUBLIC"."DOMAIN1" failed
        -Domain "PUBLIC"."DOMAIN1" is used in procedure "PUBLIC"."PKG"."PROC2" (parameter name "I") and cannot be dropped
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches=['-q'], combine_output = True, input = test_sql)
    assert act.clean_stdout == act.clean_expected_stdout
