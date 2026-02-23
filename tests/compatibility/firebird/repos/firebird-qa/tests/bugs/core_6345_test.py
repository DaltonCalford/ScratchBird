#coding:utf-8

"""
ID:          issue-6586
ISSUE:       6586
TITLE:       Server crashes on overflow of division result
DESCRIPTION:
NOTES:
[27.07.2021]
  separated code for FB 4.x+ because of fix #6874
  ("Literal 65536 (interpreted as int) can not be multiplied by itself w/o cast if result more than 2^63-1"):
  no more error with SQLSTATE = 22003 after this fix.
JIRA:        CORE-6345
FBTEST:      bugs.core_6345
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select -922337203685477.5808/-1.0 from rdb$database;
"""

act = isql_act('db', test_script)

# version: 3.0

expected_stderr_1 = """
    Statement failed, SQLSTATE = 22003
    Integer overflow.  The result of an integer operation caused the most significant bit of the result to carry.
"""

@pytest.mark.version('>=3.0.6,<4.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr_1
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

# version: 4.0

expected_stdout_2 = """
    922337203685477.58080
"""

@pytest.mark.version('>=4.0')
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
