#coding:utf-8

"""
ID:          issue-4558
ISSUE:       4558
TITLE:       Error with IF (subfunc()) when subfunc returns a boolean
DESCRIPTION:
JIRA:        CORE-4234
FBTEST:      bugs.core_4234
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns (c integer) as
        declare variable b boolean;
        declare function f1() returns boolean as
        begin
            return true;
        end
    begin
        c = 0;
        b = f1();
        if (b) then c = 1;
        suspend;
    end
    ^

    execute block returns (c integer) as
        declare variable b boolean;
        declare function f1() returns boolean as
        begin
            return true;
        end
    begin
        c = 0;
        b = f1();
        if (f1()) then c = 2;
        suspend;
    end
    ^
"""

act = isql_act('db', test_script)

expected_stdout = """
    C                               1
    C                               2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
