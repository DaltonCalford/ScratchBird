#coding:utf-8

"""
ID:          issue-8309
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8309
TITLE:       Add ALTER PACKAGE BODY and CRAETE OR ALTER PACKAGE BODY parse rules
DESCRIPTION:
    We create package with body. Then we change its body two times:
        * using 'ALTER PACKAGE';
        * using 'CREATE OR ALTER PACKAGE' clause.
    Both changes must complete without error.
NOTES:
    [10.11.2024] pzotov
    Checked on 6.0.0.523-8ca2314.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', substitutions=[('[ \\t]+', ' ')])

@pytest.mark.version('>=6.0.0')
def test_1(act: Action):

    msg_map = {
                 0: 'Initial'
                ,1: 'Changed via "ALTER PACKAGE BODY"'
                ,2: 'Changed via "CREATE OR ALTER PACKAGE BODY"'
              }

    test_sql = f"""
        set term ^;
        set heading off
        ^
        set bail on
        ^
        create or alter package pg_test as
        begin
            function fn_dummy returns varchar(50);
        end
        ^
        recreate package body pg_test as
        begin
            function fn_dummy returns varchar(50) as
            begin
                return '{msg_map[0]}';
            end
        end
        ^
        select pg_test.fn_dummy() from rdb$database
        ^

        set bail off
        ^

        -- must NOT fail since 6.0.0.523:
        alter package body pg_test as
        begin
            function fn_dummy returns varchar(50) as
            begin
                return '{msg_map[1]}';
            end
        end
        ^
        select pg_test.fn_dummy() from rdb$database
        ^

        -- must NOT fail since 6.0.0.523:
        create or alter package body pg_test as
        begin
            function fn_dummy returns varchar(50) as
            begin
                return '{msg_map[2]}';
            end
        end
        ^
        select pg_test.fn_dummy() from rdb$database
        ^
    """

    act.expected_stdout = f"""
        {msg_map[0]}
        {msg_map[1]}
        {msg_map[2]}
    """
    act.isql(switches = ['-q'], input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

