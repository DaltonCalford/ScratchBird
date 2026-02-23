#coding:utf-8

"""
ID:          issue-4267
ISSUE:       4267
TITLE:       Value of log_sweep parameter in trace configuration is ignored by trace plugin (assumed always true)
DESCRIPTION:
  Test check TWO cases:
  1) whether log_sweep = true actually lead to logging of sweep events
  2) whether log_sweep = fales actually prevents from logging of any sweep events (which is ticket issue).
JIRA:        CORE-3934
FBTEST:      bugs.core_3934
"""

import pytest
import re
from firebird.qa import *

db = db_factory()

act = python_act('db')

def sweep_present(trace_log) -> bool:
    pattern = re.compile('\\s+sweep_(start|progress|finish)(\\s+|$)', re.IGNORECASE)
    present = False
    for line in trace_log:
        if pattern.search(line):
            present = True
            break
    return present

def check_sweep(act: Action, log_sweep: bool):
    cfg = ['time_threshold = 0',
           'log_connections = true',
           f'log_sweep = {"true" if log_sweep else "false"}',
           'log_initfini = false',
           ]
    with act.trace(db_events=cfg), act.connect_server() as srv:
        srv.database.sweep(database=act.db.db_path)

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    # Case 1 - sweep logged
    check_sweep(act, True)
    assert sweep_present(act.trace_log)
    # Case 2 - sweep not logged
    act.trace_log.clear()
    check_sweep(act, False)
    assert not sweep_present(act.trace_log)
