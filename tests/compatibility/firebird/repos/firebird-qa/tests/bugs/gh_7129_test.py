#coding:utf-8

"""
ID:          issue-7129
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7129
TITLE:       Cannot alter SQL SECURITY on package
NOTES:
    [01.03.2023] pzotov
    Confirmed bug on 4.0.1.2707, date of build: 21-jan-2022
    Checked on 4.0.3.2904, 5.0.0.964 - all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set term ^;
    create view v_test as
    select
        rdb$package_name
        ,rdb$sql_security
    from rdb$packages where rdb$system_flag = 0
    ^

    create or alter package test_pkg sql security definer as
    begin
        -- nop --
    end
    ^
    recreate package body test_pkg as
    begin
        -- nop --
    end
    ^
    commit
    ^
    select * from v_test
    ^

    ---------------- definer ==> NULL -----------------

    alter package test_pkg as
    begin
        -- nop --
    end
    ^
    commit
    ^
    select * from v_test
    ^

    ---------------- invoker ==> definer -----------------

    alter package test_pkg sql security invoker as
    begin
        -- nop --
    end
    ^
    commit
    ^
    select * from v_test
    ^

    ---------------- invoker ==> NULL -----------------

    alter package test_pkg as
    begin
        -- nop --
    end
    ^
    commit
    ^
    select * from v_test
    ^
"""

act = isql_act('db', test_script)

""

expected_stdout = """
    RDB$PACKAGE_NAME                TEST_PKG
    RDB$SQL_SECURITY                <true>

    RDB$PACKAGE_NAME                TEST_PKG
    RDB$SQL_SECURITY                <null>

    RDB$PACKAGE_NAME                TEST_PKG
    RDB$SQL_SECURITY                <false>

    RDB$PACKAGE_NAME                TEST_PKG
    RDB$SQL_SECURITY                <null>
"""

@pytest.mark.version('>=4.0.2')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
