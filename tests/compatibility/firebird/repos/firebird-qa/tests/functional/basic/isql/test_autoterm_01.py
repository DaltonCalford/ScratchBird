#coding:utf-8

"""
ID:          issue-7868
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/7868
TITLE:       SET AUTOTERM ON/OFF. Basic test.
DESCRIPTION:
NOTES:
    Checked on 6.0.0.139 (intermediate snapshot of 23-nov-2023).
    Adjusted expected output after check on 6.0.0.530.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', substitutions=[('[ \t]+',' ')] )

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    test_sql = """
        set bail on;
        set list on;
        set autoterm on;
        execute block returns (o1 varchar(255)) as
        begin
            o1 = q'^^
            bye-bye term
            ^
            ^';
            suspend;
        end;
        execute block returns (o2 varchar(255)) as
        begin
            o2 = q'!set term !;
            bye-bye term
            !
            !'
            ;
            suspend
            ;
        end
        ;
        set;
        /* following statements must not produce any output: */
        create function fn_test returns double precision as begin return pi(); end;
        create procedure sp_test returns (x double precision) as begin x = pi(); suspend; end;
        create or alter package pg_test as begin procedure pg_sp_test; function pg_fn_test returns int; end; recreate package body pg_test as begin procedure pg_sp_test as begin end function pg_fn_test returns int as begin return 1; end end;
    """

    expected_stdout = """
        O1 ^
        bye-bye term
        ^
        O2 set term !;
        bye-bye term
        !
        Print statistics: OFF
        Print per-table stats: OFF
        Print wire stats: OFF
        Echo commands: OFF
        List format: ON
        Show Row Count: OFF
        Select maxrows limit: 0
        Autocommit DDL: ON
        Access Plan: OFF
        Access Plan only: OFF
        Explain Access Plan: OFF
        Display BLOB type: 1
        Set names: NONE
        Column headings: ON
        Auto Term: ON
        Terminator: ;
        Time: OFF
        Warnings: ON
        Bail on error: ON
        Local statement timeout: 0
        Keep transaction params: ON
        SET TRANSACTION
    """
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
