#coding:utf-8

"""
ID:          issue-4769
ISSUE:       4769
TITLE:       Allow sub-routines to access variables/parameters defined at the outer/parent level [CORE4449]
NOTES:
    [01.03.2023] pzotov
    Checked on 5.0.0.964
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set term ^;
    create procedure test (i_main integer) returns (o_main int) as
        declare outer_var int;
        declare procedure sub1 returns (o_sub1 int) as
        begin
            outer_var = outer_var * outer_var - i_main * 2; -- = 3*3 - 11*2 ==> -13
            i_main = i_main + 10;  -- 11 + 10 ==> 21
            o_sub1 = outer_var;
            suspend;
        end
        declare procedure sub2 returns (o_sub2 int) as
        begin
            outer_var = outer_var * outer_var- i_main * 3; -- (-13)*(-13) - 21*3 ==> 106
            i_main = i_main + 20; -- 21+20 ==> 41
            o_sub2 = outer_var;
            suspend;
        end
    begin
        outer_var = 3;
        select o_sub1 from sub1 into :o_main;
        suspend;
        select o_sub2 from sub2 into :o_main;
        suspend;
        o_main = outer_var + i_main; -- 106 + 41 ==> 147
        suspend;
    end
    ^
    set term ;^
    select * from test(11);
"""

act = isql_act('db', test_script)

expected_stdout = """
    O_MAIN                          -13
    O_MAIN                          106
    O_MAIN                          147
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
