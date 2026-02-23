#coding:utf-8

"""
ID:          issue-4595
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/4595
TITLE:       Engine crashes in case of re-creation of an erratic package body
DESCRIPTION:
JIRA:        CORE-4271
FBTEST:      bugs.core_4271
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    set term ^;
    create or alter package pkg_sequence
    as
    begin
        procedure initialize(min_value int, max_value int, step int);
        function get_current_value returns int;
        function next_value returns int;
        function is_end returns boolean;
    end
    ^

    recreate package body pkg_sequence
    as
    begin
        function get_max returns int as
        begin
            return cast(rdb$get_context('USER_SESSION', 'MAX_VALUE') as int);
        end

        function set_max(avalue int) returns int as
        begin
            rdb$set_context('USER_SESSION', 'MAX_VALUE', avalue);
            return avalue;
        end

        function get_min returns int as
        begin
            return cast(rdb$get_context('USER_SESSION', 'MIN_VALUE') as int);
        end

        function set_min(avalue int) returns int as
        begin
            rdb$set_context('USER_SESSION', 'MIN_VALUE', avalue);
            return avalue;
        end

        function get_step returns int as
        begin
            return cast(rdb$get_context('USER_SESSION', 'STEP_VALUE') as int);
        end

        function set_step(avalue int) returns int
     as
        begin
            rdb$set_context('USER_SESSION', 'STEP_VALUE', avalue);
            return avalue;
        end

        function get_current_value returns int as
        begin
            return cast(rdb$get_context('USER_SESSION', 'CURRENT_VALUE') as int);
        end

        function set_current_value(avalue int) returns int as
        begin
            rdb$set_context('USER_SESSION', 'CURRENT_VALUE', avalue);
            return avalue;
        end

        function next_value returns int as
        begin
        if (not is_end()) then
            set_current_value(get_current_value() + get_step());
            return get_current_value();
        end

        function is_end returns boolean as
        begin
            return get_current_value() > get_max();
        end

        procedure initialize(min_value int, max_value int, step int)
     as
        begin
            set_min(min_value);
            set_max(max_value);
            set_step(step);
            set_current_value(min_value);
        end
    end
    ^

    execute block returns ( out int) as
    begin
        execute procedure pkg_sequence.initialize(10, 140, 5);
        out = pkg_sequence.get_current_value();
        suspend;
        while (not pkg_sequence.is_end()) do
        begin
            out = pkg_sequence.next_value();
            suspend;
        end
    end
    ^


    recreate package body pkg_sequence
    as
    begin

        function get_max returns int as
        begin
            return cast(rdb$get_context('USER_SESSION', 'MAX_VALUE') as int);
        end

        function set_max(avalue int) returns int as
        begin
            rdb$set_context('USER_SESSION', 'MAX_VALUE', avalue);
            return avalue;
        end

        function get_min returns int as
        begin
            return cast(rdb$get_context('USER_SESSION', 'MIN_VALUE') as int);
        end

        function set_min(avalue int) returns int as
        begin
            rdb$set_context('USER_SESSION', 'MIN_VALUE', avalue);
            return avalue;
        end

        function get_step returns int as
        begin
            return cast(rdb$get_context('USER_SESSION', 'STEP_VALUE') as int);
        end

        function set_step(avalue int) returns int as
        begin
            rdb$set_context('USER_SESSION', 'STEP_VALUE', avalue);
            return avalue;
        end

        function get_current_value returns int as
        begin
            return cast(rdb$get_context('USER_SESSION', 'CURRENT_VALUE') as int);
        end

        function set_current_value(avalue int) returns int
     as
        begin
        rdb$set_context('USER_SESSION', 'CURRENT_VALUE', avalue);
        return avalue;
        end

        function next_value returns int as
        begin
        if (not is_end()) then
            set_current_value(get_current_value() + get_step());
            return get_current_value();
        end

        function is_end returns boolean as
        begin
            return get_current_value() > get_max();
        end

        procedure initialize(min_value int, max_value int, step int) as
        begin
            set_min(min_value);
            set_max(max_value);
            set_step(step);
            set_current_value(min_value);
        end
    end
    ^

    execute block returns (out int) as
    begin
        execute procedure pkg_sequence.initialize(10, 140, 5);
        out = pkg_sequence.get_current_value();
        suspend;

        while (not pkg_sequence.is_end()) do
        begin
            out = pkg_sequence.next_value();
            suspend;
        end
    end
    ^
"""

act = isql_act('db', test_script)

expected_stdout = """
    OUT                             10
    OUT                             15
    OUT                             20
    OUT                             25
    OUT                             30
    OUT                             35
    OUT                             40
    OUT                             45
    OUT                             50
    OUT                             55
    OUT                             60
    OUT                             65
    OUT                             70
    OUT                             75
    OUT                             80
    OUT                             85
    OUT                             90
    OUT                             95
    OUT                             100
    OUT                             105
    OUT                             110
    OUT                             115
    OUT                             120
    OUT                             125
    OUT                             130
    OUT                             135
    OUT                             140
    OUT                             145
    OUT                             10
    OUT                             15
    OUT                             20
    OUT                             25
    OUT                             30
    OUT                             35
    OUT                             40
    OUT                             45
    OUT                             50
    OUT                             55
    OUT                             60
    OUT                             65
    OUT                             70
    OUT                             75
    OUT                             80
    OUT                             85
    OUT                             90
    OUT                             95
    OUT                             100
    OUT                             105
    OUT                             110
    OUT                             115
    OUT                             120
    OUT                             125
    OUT                             130
    OUT                             135
    OUT                             140
    OUT                             145
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
