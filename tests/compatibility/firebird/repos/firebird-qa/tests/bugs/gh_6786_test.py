#coding:utf-8

"""
ID:          issue-6786
ISSUE:       6786
TITLE:       Add session time zone to system context
DESCRIPTION:
  Test checks only presence of not-null context variable in the 'SYSTEM' namespace,
  without verifying its value (obviously, it can vary on different machines).
  Name of context variable: 'SESSION_TIMEZONE'.
FBTEST:      bugs.gh_6786
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select iif(rdb$get_context('SYSTEM','SESSION_TIMEZONE') is not null, 'Defined.','UNDEFINED,') as session_tz from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    SESSION_TZ                      Defined.
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
