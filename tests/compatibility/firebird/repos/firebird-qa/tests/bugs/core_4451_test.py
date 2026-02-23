#coding:utf-8

"""
ID:          issue-4771
ISSUE:       4771
TITLE:       Allow output to trace explain plan form.
DESCRIPTION:
JIRA:        CORE-4451
FBTEST:      bugs.core_4451
NOTES:
    [29.06.2025] pzotov
    Suppressed name of table because on 6.x it is prefixed by SQL schema and is enclosed in quotes.
    For this test it is enough just to show that explained form of plan presents in the trace.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test(x int);
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('[ \t]+', ' '), ('[ \t]+[\\d]+[ \t]+ms', ''), ('Table.*', 'Table')])

expected_stdout = """
    Select Expression
    -> Aggregate
    -> Table "TEST" Full Scan
"""

trace = ['time_threshold = 0',
         'log_initfini = false',
         'print_plan = true',
         'explain_plan = true',
         'log_statement_prepare = true',
         'include_filter=%(from|join)[[:whitespace:]]test%',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    with act.trace(db_events=trace):
        act.isql(switches=[], input='select count(*) from test;')
    # Process trace
    show_line = 0
    for line in act.trace_log:
        show_line = (show_line + 1 if ('^' * 79) in line or show_line>0 else show_line)
        if show_line > 1:
            print(line)
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
