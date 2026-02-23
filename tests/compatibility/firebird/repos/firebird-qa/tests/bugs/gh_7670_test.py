#coding:utf-8

"""
ID:          issue-7670
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7670
TITLE:       Cursor name can duplicate parameter and variable names in procedures and functions
DESCRIPTION:
NOTES:
        Confirmed bug on 4.0.3.2957, 5.0.0.1100: all statements from this test did not issue error.
        Checked on 4.0.3.2966, 5.0.0.1121: all OK.
    [04.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.909; 5.0.3.1668.
"""

import pytest
from firebird.qa import *

db = db_factory()

# All following DDL statements must FAIL with:
# SQLSTATE = 42000 / <...> failed ... -SQL error code = -637 / -duplicate specification of ... - not supported

test_script = f"""
    set term ^;
    create function f1 (a_name_in_standalone_func integer) returns integer
    as
        declare a_name_in_standalone_func cursor for (select 1 n from rdb$database);
        declare y integer;
        declare y cursor for (select 1 n from rdb$database);
    begin
    end
    ^

    create procedure p1 (a_name_in_standalone_proc integer)
    as
        declare a_name_in_standalone_proc cursor for (select 1 n from rdb$database);
        declare y integer;
        declare y cursor for (select 1 n from rdb$database);
    begin
    end
    ^

    create procedure p2 returns (o_name_in_standalone_proc integer)
    as
        declare o_name_in_standalone_proc cursor for (select 1 n from rdb$database);
        declare y integer;
        declare y cursor for (select 1 n from rdb$database);
    begin
    end
    ^

    create or alter package pg1 as
    begin
        function pg_sum(a_name_in_packaged_func int) returns double precision;
    end
    ^
    recreate package body pg1 as
    begin
        function pg_sum(a_name_in_packaged_func int) returns double precision as
            declare a_name_in_packaged_func cursor for (select 1 n from rdb$database);
        begin
            return sqrt(a_name_in_packaged_func);
        end
    end
    ^

    create or alter package pg2 as
    begin
        procedure pg_sum(a_name_in_packaged_proc int);
    end
    ^
    recreate package body pg2 as
    begin
        procedure pg_sum(a_name_in_packaged_proc int) as
            declare a_name_in_packaged_proc cursor for (select 1 n from rdb$database);
            declare y double precision;
        begin
            y = sqrt(a_name_in_packaged_proc);
        end
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=4.0.2')
def test_1(act: Action):

    expected_stdout_5x = """
        Statement failed, SQLSTATE = 42000
        CREATE FUNCTION F1 failed
        -Dynamic SQL Error
        -SQL error code = -637
        -duplicate specification of A_NAME_IN_STANDALONE_FUNC - not supported
        Statement failed, SQLSTATE = 42000
        CREATE PROCEDURE P1 failed
        -Dynamic SQL Error
        -SQL error code = -637
        -duplicate specification of A_NAME_IN_STANDALONE_PROC - not supported
        Statement failed, SQLSTATE = 42000
        CREATE PROCEDURE P2 failed
        -Dynamic SQL Error
        -SQL error code = -637
        -duplicate specification of O_NAME_IN_STANDALONE_PROC - not supported
        Statement failed, SQLSTATE = 42000
        RECREATE PACKAGE BODY PG1 failed
        -Dynamic SQL Error
        -SQL error code = -637
        -duplicate specification of A_NAME_IN_PACKAGED_FUNC - not supported
        Statement failed, SQLSTATE = 42000
        RECREATE PACKAGE BODY PG2 failed
        -Dynamic SQL Error
        -SQL error code = -637
        -duplicate specification of A_NAME_IN_PACKAGED_PROC - not supported
    """

    expected_stdout_6x = """
        Statement failed, SQLSTATE = 42000
        CREATE FUNCTION "PUBLIC"."F1" failed
        -Dynamic SQL Error
        -SQL error code = -637
        -duplicate specification of "A_NAME_IN_STANDALONE_FUNC" - not supported
        Statement failed, SQLSTATE = 42000
        CREATE PROCEDURE "PUBLIC"."P1" failed
        -Dynamic SQL Error
        -SQL error code = -637
        -duplicate specification of "A_NAME_IN_STANDALONE_PROC" - not supported
        Statement failed, SQLSTATE = 42000
        CREATE PROCEDURE "PUBLIC"."P2" failed
        -Dynamic SQL Error
        -SQL error code = -637
        -duplicate specification of "O_NAME_IN_STANDALONE_PROC" - not supported
        Statement failed, SQLSTATE = 42000
        RECREATE PACKAGE BODY "PUBLIC"."PG1" failed
        -Dynamic SQL Error
        -SQL error code = -637
        -duplicate specification of "A_NAME_IN_PACKAGED_FUNC" - not supported
        Statement failed, SQLSTATE = 42000
        RECREATE PACKAGE BODY "PUBLIC"."PG2" failed
        -Dynamic SQL Error
        -SQL error code = -637
        -duplicate specification of "A_NAME_IN_PACKAGED_PROC" - not supported
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
