#coding:utf-8

"""
ID:          issue-2473
ISSUE:       2473
TITLE:       Parameters order of EXECUTE BLOCK statement is reversed if called from EXECUTE STATEMENT
DESCRIPTION:
JIRA:        CORE-2036
FBTEST:      bugs.core_2036
NOTES:
    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow to specify THE SAME terminator twice,
    i.e.
    set term @; select 1 from rdb$database @ set term @; - will not compile ("Unexpected end of command" raises).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns(p1 int, p2 int, p3 int) as
        declare s varchar(255);
    begin
        s =    'execute block ( i1 int = ?, i2 int = ?, i3 int = ? ) returns(o1 int, o2 int, o3 int) as '
            || 'begin '
            || '    o1 = i1 * 2; '
            || '    o2 = i2 * 4; '
            || '    o3 = i3 * 8; '
            || '    suspend; '
            || 'end '
        ;
        execute statement (s) (654, 543, 432) into p1, p2, p3;
        suspend;
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    P1                              1308
    P2                              2172
    P3                              3456
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

