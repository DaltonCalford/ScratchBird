#coding:utf-8

"""
ID:          issue-1781
ISSUE:       1781
TITLE:       ISQL crash when converted-from-double string longer than 23 bytes
DESCRIPTION:
JIRA:        CORE-1363
FBTEST:      bugs.core_1363
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select -2.488355210669293e-22 from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
               CONSTANT
=======================
 -2.488355210669293e-22

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

