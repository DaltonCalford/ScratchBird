#coding:utf-8

"""
ID:          gtcs.test_fb_sql_for_range
TITLE:       Range-based FOR statement.
DESCRIPTION:
    Functionality descriprion: https://github.com/FirebirdSQL/firebird/issues/8498
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_FOR_RANGE.script
    Commit:
    https://github.com/FirebirdSQL/firebird/commit/f5b6b0c0fe7595ddee5915328774f2cc10384384
NOTES:
    [06.04.2024] pzotov
    Other checks/examples will be added in bugs/tests/test_8498.py
    Checked on 6.0.0.717-f5b6b0c (intermediate snapshot).
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[ ('[ \\t]+', ' ') ])

expected_stdout = """
    SP1_OUT_A 3
    SP1_OUT_A 7
    SP1_OUT_A 10
    SP1_OUT_A 12
    SP1_OUT_A 14
    SP1_OUT_A 16
    SP1_OUT_A -18

    SP1_OUT_B 11
    SP1_OUT_B 13
    SP1_OUT_B 15
    SP1_OUT_B -17

    SP2_OUT 3
    SP2_OUT 4
    SP2_OUT 5
    SP2_OUT 5

    EB1_OUT 10.00
    EB1_OUT 8.90
    EB1_OUT 7.80
    EB1_OUT 6.70
    EB1_OUT 5.60
    EB1_OUT 4.50
    EB1_OUT 3.40
    EB1_OUT 2.30
    EB1_OUT 1.20
    EB1_OUT -0.10

    EB2_OUT 1
    EB2_OUT 1
    EB2_OUT 2
    EB2_OUT 2
    EB2_OUT 3
    EB2_OUT 3
    EB2_OUT 4
    EB2_OUT 4
    EB2_OUT 5
    EB2_OUT 5
    EB2_OUT 6
    EB2_OUT 6
    EB2_OUT 7
    EB2_OUT 7
    EB2_OUT 8
    EB2_OUT 8
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    test_sql = """
        set list on;
        set term ^;
        create or alter procedure p1 (init integer) returns (sp1_out integer)
        as
        begin
            sp1_out = init;
            for sp1_out = sp1_out to 16 by 2 do
            begin
                if (sp1_out = 5) then
                    continue;
                else if (sp1_out = 9) then
                    sp1_out = sp1_out + 1;

                suspend;
            end

            sp1_out = -sp1_out;
            suspend;
        end^
        select p.sp1_out as sp1_out_a from p1(3) as p
        ^
        select p.sp1_out as sp1_out_b from p1(11) as p
        ^

        create or alter procedure p2 (init integer) returns (sp2_out integer)
        as
            declare finish integer = 5;
            declare last integer;

            declare procedure sub1 returns (sp2_out integer)
            as
            begin
                for sp2_out = init to finish do
                begin
                    last = sp2_out;
                    suspend;
                end
            end
        begin
            for select sp2_out from sub1 into :sp2_out do
                suspend;

            sp2_out = last;
            suspend;
        end^
        select * from p2(3)
        ^

        execute block returns (eb1_out numeric(5,2))
        as
            declare init integer = 10;
            declare finish integer = 1;
            declare by_val numeric(5,2) = 1.1;
        begin
            for eb1_out = :init downto :finish by :by_val do
            begin
                init = init + 1;
                finish = finish + 1;
                by_val = by_val + 1;
                suspend;
            end

            eb1_out = -eb1_out;
            suspend;
        end
        ^

        execute block returns (eb2_out integer)
        as
        begin
            for eb2_out = null to 10 do
                suspend;

            for eb2_out = 1 to null do
                suspend;

            for eb2_out = 1 to 10 by null do
                suspend;
        end
        ^
        execute block returns (eb2_out integer)
        as
            declare i integer;
        begin
            outer_for: for eb2_out = 1 to 8 do
            begin
                for i = 1 to 5 do
                begin
                    if (i = 3) then
                        continue outer_for;
                    suspend;
                end
            end
        end
        ^
    """
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_sql, combine_output = True )

    assert act.clean_stdout == act.clean_expected_stdout
