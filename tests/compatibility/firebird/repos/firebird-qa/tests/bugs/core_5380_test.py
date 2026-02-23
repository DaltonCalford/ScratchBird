#coding:utf-8

"""
ID:          issue-5653
ISSUE:       5653
TITLE:       Allow subroutines to call others subroutines and themself recursively
DESCRIPTION:
  We check not only ability of recursive calls but also max depth of them. It should be equal to 1000.
JIRA:        CORE-5380
FBTEST:      bugs.core_5380
    [30.06.2025] pzotov
    Part of call stack ('At sub function <FN_NAME> line X col Y') must be supressed because its length is limited to 1024 characters
    and number of lines (together with interrupting marker '...') depends on length of function name that is called recursively.
    It is enough for this test to check only presense of 'SQLSTATE = 54001' and 'Too many concurrent executions' in STDERR.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter function get_arithmetic_progression_total(n smallint) returns decfloat(34)  as
        declare function get_sub_total_recursively(n smallint) returns decfloat(34) as
        begin
            if (n = 1) then
                return 1;
            else
                return n + get_sub_total_recursively(n - 1);
        end
    begin
        if (n <= 0) then
            return 0;
        else
            return get_sub_total_recursively(n);
    end
    ^
    set term ;^
    commit;

    set list on;
    select get_arithmetic_progression_total(1001) as arithmetic_progression_total from rdb$database;
    select get_arithmetic_progression_total(1002) as arithmetic_progression_total from rdb$database;
    -- (a1 + an) * n / 2
"""

substitutions = [ ('^((?!(SQLSTATE|Too many concurrent executions|ARITHMETIC_PROGRESSION_TOTAL)).)*$', ''), ('[ \t]+', ' ') ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    ARITHMETIC_PROGRESSION_TOTAL 501501
    Statement failed, SQLSTATE = 54001
    Too many concurrent executions of the same request
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

