#coding:utf-8

"""
ID:          issue-4221
ISSUE:       4221
TITLE:       Server crashes on preparing empty query when trace is enabled
DESCRIPTION:
    Crash can be reproduced if FB 2.5.0 or 2.5.1 runs in Classic or SuperClassic mode (Super not affected).
    Steps to reproduce:
      0. Prepare trace configuration and launch user trace;
          time_threshold = 0
          log_statement_prepare true
      1. Launch ISQL and execute execute block with empty ES;

    FB 2.5.1.26351 crashed at this point. NB: there is no crash if we do not launch trace.

    Test checks that:
    1. ISQL actually produces error messages which do NOT contain
       'SQLSTATE = 08006 / Unable to complete network ... / -Error reading data ...'
    2. firebird.log remain unchanged (we compare its content before and after ISQL).

JIRA:        CORE-3884
FBTEST:      bugs.core_3884
NOTES:
    [28.05.2022] pzotov
    Checked on 5.0.0.501, 4.0.1.2692, 3.0.8.33535
"""

import pytest
from firebird.qa import *
import locale
from difflib import unified_diff
import re

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' '), ('line(:)? \\d+, col(umn)?(:)? \\d+', '')])

expected_stdout_isql = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Unexpected end of command - line 1, column 1
    -At block line: 3, col: 13
"""

@pytest.mark.trace
@pytest.mark.version('>=3')
def test_1(act: Action, capsys):

    trace_cfg_items = [
        'time_threshold = 0',
        'log_statement_prepare = true',
    ]

    sql_empty_sttm = '''
        set term ^;
        execute block as
        begin
            execute statement '';
        end
        ^
    '''

    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.info.get_log()
        fb_log_init = srv.readlines()


    with act.trace(db_events = trace_cfg_items, encoding=locale.getpreferredencoding()):
        act.isql(input = sql_empty_sttm, combine_output = True)

    act.expected_stdout = expected_stdout_isql
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #------------------------------------------------------------------------------------

    with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
        srv.info.get_log()
        fb_log_curr = srv.readlines()

    for line in unified_diff(fb_log_init, fb_log_curr):
        if line.startswith('+'):
            print(line.strip())

    assert capsys.readouterr().out == ''
