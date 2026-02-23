#coding:utf-8

"""
ID:          issue-5972
ISSUE:       5972
TITLE:       Trace config with misplaced "{" lead firebird to crash
DESCRIPTION:
  We create trace config with following INVALID content:
    database = (%[\\/](security[[:digit:]]).fdb|(security.db))
    enabled = false
    {
   }

    database =
    {
      enabled = true
      log_connections = true
    }

  Then we run new process with ISQL with connect to test DB.
  This immediately should cause raise error in the 1st (trace) process:
    1  Trace session ID 1 started
    2  Error creating trace session for database "C:\\MIX\\FIREBIRD\\FB30\\SECURITY3.FDB":
    3  error while parsing trace configuration
    4  Trace parameters are not present

  It was encountered that in FB 3.0.3 Classic lines 2..4 appear TWICE. See note in the ticket, 16/Jan/18 05:08 PM
JIRA:        CORE-5706
FBTEST:      bugs.core_5706
NOTES:
    [15.09.2022] pzotov
    Full trace config must be passed to act.trace() rather than only trace items.
    Checked on Linux and Windows: 3.0.8.33535, 4.0.1.2692
"""
import locale
from difflib import unified_diff

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

trace_conf = """
# ::: NOTE :::
# First 'database' section here INTENTIONALLY was written WRONG!
database = (%[\\\\/](security[[:digit:]]).fdb|(security.db))
enabled = false
{
}

database =
{
  enabled = true
  log_connections = true
}
"""

@pytest.mark.trace
@pytest.mark.version('>=3.0.3')
def test_1(act: Action):
    log_before = act.get_firebird_log()
    with act.trace(config=trace_conf, keep_log=False):
        # We run here ISQL only in order to "wake up" trace session and force it to raise error in its log.
        # NO message like 'Statement failed, SQLSTATE = 08004/connection rejected by remote interface' should appear now!
        act.isql(switches=['-n', '-q'], input='quit;')
        log_after = act.get_firebird_log()
    assert list(unified_diff(log_before, log_after)) == []
