#coding:utf-8

"""
ID:          issue-1686
ISSUE:       1686
TITLE:       GSec incorrectly processes some switches
DESCRIPTION:
JIRA:        CORE-1263
FBTEST:      bugs.core_1263
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stderr_a = """
GSEC> invalid switch specified in interactive mode
GSEC> invalid switch specified in interactive mode
GSEC> invalid switch specified in interactive mode
GSEC> invalid switch specified in interactive mode
GSEC> invalid switch specified in interactive mode
GSEC> invalid switch specified
error in switch specifications
GSEC>
"""

commands = """add BADPARAM -pa PWD
add BADPARAM -pas PWD
add BADPARAM -password PWD
add BADPARAM -user USR
add BADPARAM -database DB
add BADPARAM -trusted
quit
"""

@pytest.mark.version('>=3.0')
@pytest.mark.platform('Linux', 'Darwin')
def test_1_a(act: Action):
    act.expected_stderr = expected_stderr_a
    act.gsec(input=commands)
    assert act.clean_stderr == act.clean_expected_stderr

expected_stderr_b = """
GSEC> invalid switch specified in interactive mode
GSEC> invalid switch specified in interactive mode
GSEC> invalid switch specified in interactive mode
GSEC> invalid switch specified in interactive mode
GSEC> invalid switch specified in interactive mode
GSEC> invalid switch specified in interactive mode
GSEC>
"""

@pytest.mark.version('>=3.0')
@pytest.mark.platform('Windows')
def test_1_b(act: Action):
    act.expected_stderr = expected_stderr_b
    act.gsec(input=commands)
    assert act.clean_stderr == act.clean_expected_stderr



