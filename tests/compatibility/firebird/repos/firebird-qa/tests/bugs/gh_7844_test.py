#coding:utf-8

"""
ID:          issue-7844
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7844
TITLE:       Removing first column with SET WIDTH crashes ISQL
DESCRIPTION:
NOTES:
   [14.11.2023] pzotov
   There are no crashes on FB 3.x ... 6.x neither on Windows nor on Linux.
   Crash occurred only in DEBUG build (reply from Adriano, 14.11.2023 12:18).
   On release build, 'set width a;' caused missing info about width for column 'b'.
   Confirmed bug on WI-T6.0.0.122.
   Checked on 6.0.0.124 (intermediate build, timestamp: 14.11.2023 09:00).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;

    recreate table test(a varchar(10), b varchar(28));

    insert into test(a,b) values('no special', 'chars must be here!@#$%^&*()');
    commit;

    set width a 10;
    set width b 18;

    select 'point-1', t.* from test t;

    -- This caused width of 'b' to be restored to its original size
    -- (instead of preserving its value to that was specified in 'set with b ...'):
    set width a;

    select 'point-2', t.* from test t;

    -- crash here, but only in DEBUG build.
    set width a 10;

    select 'point-3', t.* from test t;
"""
expected_stdout = """
    point-1  no special chars must be here
    point-2  no special chars must be here
    point-3  no special chars must be here
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.12')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
