#coding:utf-8

"""
ID:          issue-4421
ISSUE:       4421
TITLE:       Server crashes while converting an overscaled numeric to a string
DESCRIPTION:
JIRA:        CORE-4093
FBTEST:      bugs.core_4093
"""

import pytest
from firebird.qa import *

db = db_factory()

# version: 3.0

test_script_1 = """
    set heading off;
    select cast(cast(0 as numeric(18, 15)) * cast(0 as numeric(18, 15)) * cast(0 as numeric(18, 15)) as varchar (41)) from rdb$database;
"""

act_1 = isql_act('db', test_script_1)

expected_stderr_1 = """
    Statement failed, SQLSTATE = 22018
    conversion error from string "0.000000000000000000000000000000000000000000000"
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stderr = expected_stderr_1
    act_1.execute()
    assert act_1.clean_stderr == act_1.clean_expected_stderr

# version: 4.0

test_script_2 = """
    set heading off;
    select cast( cast(0 as numeric(38, 38)) * cast(0 as numeric(38, 38)) * cast(0 as numeric(38, 38)) as varchar(100) ) from rdb$database;
    select cast( cast(0 as numeric(38, 38)) * cast(0 as numeric(38, 38)) * cast(0 as numeric(38, 38)) as numeric(38, 38) ) from rdb$database;
"""

act_2 = isql_act('db', test_script_2)

expected_stdout_2 = """
    0E-114
    0.00000000000000000000000000000000000000
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout_2
    act_2.execute()
    assert act_2.clean_stdout == act_2.clean_expected_stdout

