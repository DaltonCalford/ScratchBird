#coding:utf-8

"""
ID:          issue-3948
ISSUE:       3948
TITLE:       Include expected and actual string lenght into error message for sqlcode -802
DESCRIPTION:
JIRA:        CORE-3594
FBTEST:      bugs.core_3594
NOTES:
    [27.06.2025] pzotov
    Suppressed output of procedure name (no needed for this test).

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure sp_overflowed_1 as begin end;
    set term ^;
    create or alter procedure sp_detailed_info returns(msg varchar(60)) as
    begin
        msg = '....:....1....:....2....:....3....:....4....:....5....:....6';
        suspend;
    end
    ^

    create or alter procedure sp_overflowed_1 returns(msg varchar(50)) as
    begin
        execute procedure sp_detailed_info returning_values msg;
        suspend;
    end

    ^
    create or alter procedure sp_overflowed_2 returns(msg varchar(59)) as
    begin
        msg = '....:....1....:....2....:....3....:....4....:....5....:....6';
        suspend;
    end
    ^
    set term ;^
    commit;

    set heading off;
    select * from sp_overflowed_1;
    select * from sp_overflowed_2;

    -- On 2.5.x info about expected and actual length is absent:
    -- Statement failed, SQLSTATE = 22001
    -- arithmetic exception, numeric overflow, or string truncation
    -- -string right truncation
"""

substitutions=[('line: .*', 'line'), ('col: .*', 'col'), ('(-)?At procedure .*', 'At procedure')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 50, actual 60
    At procedure

    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 59, actual 60
    At procedure
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

