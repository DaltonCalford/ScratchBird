#coding:utf-8

"""
ID:          issue-7590
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7590
TITLE:       Improve DECLARE VARIABLE to accept complete value expressions
DESCRIPTION:
    Test only ckecks ability to use feature as it is described in the doc.
    More complex tests will be implemented later.
NOTES:
    [02.10.2023] pzotov
    Checked on 6.0.0.65.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns (o1 integer, o2 integer) as

        declare function sf1 returns integer;
        declare v0 integer = sf1();
        declare v1 integer;
        declare v2 integer = 2;

        declare function sf1 returns integer
        as
        begin
            v1 = 10;
            v2 = 20;
            return 0;
        end
    begin
        o1 = v1;
        o2 = v2;
        suspend;
    end
    ^

    -- It's an error if a subroutine reads a variable declared after the one being initialized like in the following example.
    -- When sf1 is called, v1 is not yet initialized.
    execute block returns(msg varchar(20)) as
        declare function sf1 returns integer;

        declare v0 integer = sf1();
        declare v1 integer;

        declare function sf1 returns integer
        as
        begin
            return v1;
        end
    begin
        msg = 'UNEXPECTEDLY successful execution.';
        suspend;
    end
    ^
"""
substitutions = [ ('[ \t]+', ' '), ('[-]?At\\s+.* line: \\d+, col: \\d+' , 'At line/col') ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    O1 <null>
    O2 2
    Statement failed, SQLSTATE = 42000
    Variable [number 2] is not initialized
    -At sub function 'SF1' line: 10, col: 13
    At block line: 5, col: 27
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
