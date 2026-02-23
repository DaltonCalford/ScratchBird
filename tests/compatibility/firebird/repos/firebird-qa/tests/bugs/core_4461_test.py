#coding:utf-8

"""
ID:          issue-4781
ISSUE:       4781
TITLE:       nbackup prints error messages to stdout instead stderr
DESCRIPTION:
JIRA:        CORE-4461
FBTEST:      bugs.core_4461
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('Failure: Database error', '')])

expected_stderr = """
  [
   PROBLEM ON "attach database".
   Your user name and password are not defined. Ask your database administrator to set up a Firebird login.
   SQLCODE:-902
  ]
"""

@pytest.mark.version('>=2.5.4')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.nbackup(switches=['-user', 'nonExistentFoo', '-password', 'invalidBar',
                          '-L', act.db.dsn], credentials=False)
    assert act.clean_stderr == act.clean_expected_stderr


