#coding:utf-8

"""
ID:          issue-5423
ISSUE:       5423
TITLE:       Wrong error message when user tries to set number of page buffers into not supported value
DESCRIPTION:
JIRA:        CORE-5140
FBTEST:      bugs.core_5140
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('range.*', 'range')])

expected_stderr = """
    bad parameters on attach or create database
    -Attempt to set in database number of buffers which is out of acceptable range [50:131072]
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.gfix(switches=['-b', '1', act.db.dsn])
    assert act.clean_stderr == act.clean_expected_stderr


