#coding:utf-8

"""
ID:          issue-6165
ISSUE:       6165
TITLE:       Regression: can not launch trace if its 'database' section contains regexp pattern with curvy brackets to enclose quantifier
DESCRIPTION:
  Database file name for check: {core_5907.97}.tmp // NB: outer curvy brackets ARE INCLUDED in this name.
  This name should match to pattern: (\\{core_5907.[[:DIGIT:]]{2}\\}).tmp -- but we have to duplicate every "{" and "}".
  Also, we have to duplicate '' otherwise it will be escaped by fbtest framework.
JIRA:        CORE-5907
FBTEST:      bugs.core_5907
"""

import pytest
from firebird.qa import *

substitutions = [('.*{CORE_5907.97}.FDB', '{CORE_5907.97}.FDB'),
                 ('.*{core_5907.97}.fdb', '{CORE_5907.97}.FDB')]

init_script = """
    recreate table test(id int);
    commit;
"""

db = db_factory(init=init_script, filename='{core_5907.97}.fdb')

act = python_act('db', substitutions=substitutions)

expected_stdout = """
    {CORE_5907.97}.FDB
    Found expected ATTACH.
    Found expected DETACH.
"""

trace_conf = ['database=(%[\\\\/](\\{{core_5907.[[:DIGIT:]]{{2}}\\}}).fdb)',
              '{',
              'enabled = true',
              'time_threshold = 0',
              'log_connections = true',
              'log_initfini = false',
              '}'
              ]

@pytest.mark.trace
@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    with act.trace(config=trace_conf):
        act.isql(switches=[], input='set list on;select mon$database_name from mon$database;')
        print(act.stdout)
    #
    for line in act.trace_log:
        if 'ATTACH_DATABASE' in line:
            print('Found expected ATTACH.')
        if 'DETACH_DATABASE' in line:
            print('Found expected DETACH.')
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
