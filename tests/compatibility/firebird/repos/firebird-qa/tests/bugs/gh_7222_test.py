#coding:utf-8

"""
ID:          issue-7220
ISSUE:       7220
TITLE:       Dependencies of packaged functions are not tracked
DESCRIPTION:
NOTES:
    [24.02.2023] pzotov
    See also #7220
    Confirmed bug on 5.0.0.520, 4.0.2.2780
    Checked on 5.0.0.958, 4.0.3.2903 - all fine.
"""
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

test_sql = """
    set term ^;
    create domain domain1 integer
    ^
    create domain domain2 integer
    ^
    create package pkg
    as
    begin
        procedure proc1;
        function func1 returns integer;
    end
    ^
    recreate package body pkg
    as
    begin
        procedure proc1
        as
            declare v domain1;
        begin
        end

        function func1 returns integer
        as
            declare v domain2;
        begin
        end
    end
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
"""

expected_out = """
    RDB$DEPENDENT_NAME              PKG
    RDB$DEPENDED_ON_NAME            DOMAIN1

    RDB$DEPENDENT_NAME              PKG
    RDB$DEPENDED_ON_NAME            DOMAIN2

    Records affected: 2
"""

@pytest.mark.version('>=4.0.2')
def test_1(act: Action):

    act.expected_stdout = expected_out
    act.isql(switches=['-q'], combine_output = True, input = test_sql)
    assert act.clean_stdout == act.clean_expected_stdout
