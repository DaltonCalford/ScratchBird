#coding:utf-8

"""
ID:          n/a
TITLE:       Check PSQL range-based loops.
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_FOR_RANGE.script

    Documentation:
    $FB_HOME/doc/sql.extensions/README.range_based_for.md
NOTES:
    [06.09.2025] pzotov
    Checked on 6.0.0.1261
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set term ^;
    create or alter procedure p1 (init integer) returns (out integer)
    as
    begin
        out = init;

        for out = out to 16 by 2 do
        begin
            if (out = 5) then
                continue;
            else if (out = 9) then
                out = out + 1;

            suspend;
        end

        out = -out;
        suspend;
    end^

    select * from p1(3)^
    select * from p1(11)^


    create or alter procedure p2 (init integer) returns (out integer)
    as
        declare finish integer = 5;
        declare last integer;

        declare procedure sub1 returns (out integer)
        as
        begin
            for out = init to finish do
            begin
                last = out;
                suspend;
            end
        end
    begin
        for select out from sub1 into :out do
            suspend;

        out = last;
        suspend;
    end^

    select * from p2(3)^


    execute block returns (out numeric(5,2))
    as
        declare init integer = 10;
        declare finish integer = 1;
        declare by_val numeric(5,2) = 1.1;
    begin
        for out = :init downto :finish by :by_val do
        begin
            init = init + 1;
            finish = finish + 1;
            by_val = by_val + 1;
            suspend;
        end

        out = -out;
        suspend;
    end^


    execute block returns (out integer)
    as
    begin
        for out = null to 10 do
            suspend;

        for out = 1 to null do
            suspend;

        for out = 1 to 10 by null do
            suspend;
    end^


    execute block returns (out integer)
    as
        declare i integer;
    begin
        outer_for: for out = 1 to 8 do
        begin
            for i = 1 to 5 do
            begin
                if (i = 3) then
                    continue outer_for;
                suspend;
            end
        end
    end^
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    OUT 3
    OUT 7
    OUT 10
    OUT 12
    OUT 14
    OUT 16
    OUT -18
    OUT 11
    OUT 13
    OUT 15
    OUT -17
    OUT 3
    OUT 4
    OUT 5
    OUT 5
    OUT 10.00
    OUT 8.90
    OUT 7.80
    OUT 6.70
    OUT 5.60
    OUT 4.50
    OUT 3.40
    OUT 2.30
    OUT 1.20
    OUT -0.10
    OUT 1
    OUT 1
    OUT 2
    OUT 2
    OUT 3
    OUT 3
    OUT 4
    OUT 4
    OUT 5
    OUT 5
    OUT 6
    OUT 6
    OUT 7
    OUT 7
    OUT 8
    OUT 8
"""

@pytest.mark.version('>=6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
