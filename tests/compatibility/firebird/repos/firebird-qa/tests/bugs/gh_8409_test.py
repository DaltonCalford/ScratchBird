#coding:utf-8

"""
ID:          issue-8409
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8409
TITLE:       Error message "SQL -104 / Unexpected end of command" appears in a trace log when 'SET AUTOTERM ON;' is used
DESCRIPTION: Test checks that trace will not contain error messages caused by DDL prepare. Expected output is empty.
NOTES:
    [23.01.2025] pzotov
    Checked on 6.0.0.595-2c5b146 (intermediate snapshot, UTC 20250123 01:49).
"""
import re

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

test_sql = """
set autoterm on;
create procedure sp_test1 as
    declare n smallint;
begin
    n = 1;
end;create procedure sp_test2 as declare n smallint;begin n = 2;end

;
"""

trace_events_lst = \
    [ 'time_threshold = 0'
      ,'log_errors = true'
      ,'log_statement_prepare = true'
      ,'log_initfini = false'
    ]

@pytest.mark.trace
@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    with act.trace(db_events = trace_events_lst):
        act.reset()
        act.isql(switches = ['-q'], input = test_sql, combine_output = True)

    allowed_patterns = [
        'ERROR AT JStatement::prepare'
        ,'335544569 : Dynamic SQL Error'
        ,'335544436 : SQL error code = -104'
        ,'335544851 : Unexpected end of command'
    ]
    allowed_patterns = [re.compile(p, re. IGNORECASE) for p in allowed_patterns]


    for line in act.trace_log:
        for p in allowed_patterns:
             if p.search(line):
                 print('UNEXPECTED: '+line)

    act.expected_stdout = ""
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
