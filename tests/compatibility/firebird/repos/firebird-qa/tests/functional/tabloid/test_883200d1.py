#coding:utf-8

"""
ID:          None
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/883200d1927f74baadc7eb14293d1a9fb4e517ce
TITLE:       Do not re-prepare statements when execute DDL in ISQL
DESCRIPTION:
NOTES:
    Confirmed duplicated PREPARE_STATEMENT in 6.0.0.454
    Checked on 6.0.0.457 - all OK.
"""

import re
import time

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

expected_stdout = """
"""

@pytest.mark.trace
@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    trace_cfg_items = [
        'time_threshold = 0',
        'log_statement_prepare = true',
        'log_initfini = false',
        'log_errors = true',
    ]

    DDL_STTM = 'recreate table test(id int)'
    with act.trace(db_events = trace_cfg_items, encoding='utf-8'):
        act.isql(switches = ['-q'], input = DDL_STTM + ';', combine_output = True)
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

    allowed_patterns = ( re.escape(DDL_STTM),)
    allowed_patterns = [ re.compile(p, re.IGNORECASE) for p in allowed_patterns ]

    for line in act.trace_log:
        if line.strip():
            if act.match_any(line.strip(), allowed_patterns):
                print(line.strip())


    expected_trace_log = f"""
        {DDL_STTM}
    """
    act.expected_stdout = expected_trace_log
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
