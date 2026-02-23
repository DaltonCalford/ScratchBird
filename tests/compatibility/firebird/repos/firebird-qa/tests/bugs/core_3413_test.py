#coding:utf-8

"""
ID:          issue-3776
ISSUE:       3776
TITLE:       Improve diagnostics of internal trace errors
DESCRIPTION:
JIRA:        CORE-3413
FBTEST:      bugs.core_3413
"""

import pytest
from firebird.qa import *

substitutions = [('^((?!ERROR|ELEMENT).)*$', ''),
                 ('ERROR CREATING TRACE SESSION.*', 'ERROR CREATING TRACE SESSION'),
                 ('.*"FOO" IS NOT A VALID.*', '"FOO" IS NOT A VALID')]

db = db_factory()

act = python_act('db', substitutions=substitutions)

expected_stdout = """
    ERROR CREATING TRACE SESSION FOR DATABASE
    ERROR WHILE PARSING TRACE CONFIGURATION
    ELEMENT "LOG_STATEMENT_FINISH": "FOO" IS NOT A VALID
"""

trace = ['time_threshold = 0',
           'log_statement_finish = foo'
           ]

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    with act.trace(db_events=trace):
        act.isql(switches=['-n'], input='select 1 as c from rdb$database;')
    act.expected_stdout = expected_stdout
    act.trace_to_stdout(upper=True)
    assert act.clean_stdout == act.clean_expected_stdout
