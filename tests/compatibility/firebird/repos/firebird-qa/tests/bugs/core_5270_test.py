#coding:utf-8

"""
ID:          issue-5548
ISSUE:       5548
TITLE:       FBSVCMGR does not produce error while attempting to shutdown a database without
  specified timeout (prp_force_shutdown N)
DESCRIPTION:
JIRA:        CORE-5270
FBTEST:      bugs.core_5270
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stderr = """
    must specify type of shutdown
"""

@pytest.mark.version('>=3.0.1')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.svcmgr(switches=['action_properties', 'dbname', str(act.db.db_path),
                         'prp_shutdown_mode', 'prp_sm_single'])
    assert act.clean_stderr == act.clean_expected_stderr


