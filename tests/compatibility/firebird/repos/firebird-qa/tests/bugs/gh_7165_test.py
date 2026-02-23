#coding:utf-8

"""
ID:          issue-7165
ISSUE:       7165
TITLE:       Provide ability to see in the trace log events related to missing security context
DESCRIPTION:
    List of AuthClient plugins must contain Win_Sspi in order to reproduce this test expected results.
    Otherwise firebird.log will not contain any message like "Available context(s): ..."
    Because of this, test marked as to be performed on WINDOWS only.
    Test removed ISC_USER and ISC_PASSWORD from environment (if they were defined previously),
    this is mandatory condition for reproducing problem.
    Then we launch trace and invoke ISQL with single command: QUIT.
    ISQL must issue "SQLSTATE = 28000 / Missing security context ...", trace log must contain
    gdscode plus "Missing security context" text.
NOTES:
    [24.02.2023] pzotov
    See also bugs/core_6362_test.py
    Confirmed missed message 'Missing security context' on 5.0.0.513 (date of build: 10-JUN-2022).
    Checked on 5.0.0.958 - all OK.
"""

import os
import locale
import pytest
from firebird.qa import *

for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

test_sql = '''
    quit;
'''

db = db_factory()
substitutions = [ ( '^((?!context).)*$', ''),
                  ( 'Missing security context(\\(s\\))?( required)? for .*', 'Missing security context'),
                  ( 'Available context(\\(s\\))?(:)? .*', 'Available context'),
                  ( '[\t ]+', ' '),
                ]

act = python_act('db', substitutions = substitutions)

expected_stdout_trace = """
    335545060 : Missing security context
"""

expected_stderr_isql = """
    Statement failed, SQLSTATE = 28000
    Missing security context for TEST.FDB
"""

@pytest.mark.trace
@pytest.mark.version('>=5.0')
@pytest.mark.platform('Windows')
def test_1(act: Action, capsys):

    trace_cfg_items = [
        'time_threshold = 0',
        'log_errors = true',
        'log_statement_finish = true',
    ]

    with act.trace(db_events = trace_cfg_items, encoding=locale.getpreferredencoding()):
        act.expected_stderr = expected_stderr_isql
        act.isql(switches=['-q'], input = 'quit;', credentials = False)
        assert act.clean_stderr == act.clean_expected_stderr
        act.reset()

    rdb_tables_found_for_this_ddl = False
    for line in act.trace_log:
        if line.strip():
            print(line.strip())
            
    act.expected_stdout = expected_stdout_trace
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
