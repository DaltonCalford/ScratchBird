#coding:utf-8

"""
ID:          issue-3265
ISSUE:       3265
TITLE:       isql should show packaged procedures and functions categorized per package
DESCRIPTION:
JIRA:        CORE-2881
FBTEST:      bugs.core_2881
NOTES:
    [27.06.2025] pzotov
    Re-implemented: use variables to store scema prefix (since 6.0.0.834), packages header and body.
    Use f-notations to substitute variable values in the test script and expected_out.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'PUBLIC.'
    PKG_HEAD_1 = """
        begin
            function f(x int) returns int;
            procedure p(x int) returns(y int);
        end
    """
    PKG_BODY_1 = """
        begin
            function f(x int) returns int as
            begin
            return 22*x;
            end
            procedure p(x int) returns(y int) as
            begin
            y = 33*x;
            suspend;
            end
        end
    """

    PKG_HEAD_2 = """
        begin
            function f(x int) returns int;
            procedure p(x int) returns(y int);
        end
    """
    PKG_BODY_2 = """
        begin
            function f(x int) returns int as
            begin
            return 222*x;
            end
            procedure p(x int) returns(y int) as
            begin
            y = 333*x;
            suspend;
            end
        end
    """

    test_script = f"""
        set term ^;
        ^
        create package p1 as
        {PKG_HEAD_1}
        ^
        create package body p1 as
        {PKG_BODY_1}
        ^
        create package p2 as
        {PKG_HEAD_2}
        ^
        create package body p2 as
        {PKG_BODY_2}
        ^
        set term ;^
        commit;

        show package p1;
        show package p2;
    """

    expected_stdout = f"""
        {SQL_SCHEMA_PREFIX}P1
        Header source:
        {PKG_HEAD_1}

        Body source:
        {PKG_BODY_1}

        {SQL_SCHEMA_PREFIX}P2
        Header source:
        {PKG_HEAD_2}

        Body source:
        {PKG_BODY_2}
    """

    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
