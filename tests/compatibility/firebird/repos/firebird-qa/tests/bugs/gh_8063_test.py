#coding:utf-8

"""
ID:          issue-8063
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8063
TITLE:       (var)char variables/parameters assignments fail in Stored Procedures with subroutines
DESCRIPTION:
NOTES:
    [02.04.2024] pzotov
    Test code has been changed to be minimal reproducable.

    Confirmed bug on 6.0.0.305 8a4f691; 5.0.1.1371 295758d
    Checked on 6.0.0.305 73551f3; 5.0.1.1371 48915d1 (intermediate snapshots).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set term ^;
    create or alter procedure sp_test_1 returns (
        o_char varchar(10)
    ) sql security invoker
    as
        declare outer_v varchar(10) character set utf8; -- <<< [!] <<< charset *utf8* must be specified here to reproduce

        declare function f_inner returns int as
        begin
            outer_v = current_user;
            return 1;
        end

    begin
        outer_v = current_user;
        o_char = outer_v;
        suspend;
    end
    ^

    create or alter procedure sp_test_2 returns (
        o_char varchar(10)
    ) sql security invoker
    as
        declare outer_v varchar(10) character set utf8;
    begin
        outer_v = current_user;
        o_char = outer_v;
        suspend;
    end
    ^
    set term ^;
    commit;

    select o_char as sp_1 from sp_test_1;

    select o_char as sp_2 from sp_test_2;
"""

act = isql_act('db', test_script, substitutions=[ ('[ \\t]+', ' ') ])

@pytest.mark.version('>=5.0.1')
def test_1(act: Action):

    expected_stdout = f"""
        SP_1 {act.db.user}
        SP_2 {act.db.user}
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

