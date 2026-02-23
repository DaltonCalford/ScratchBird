#coding:utf-8

"""
ID:          issue-2209
ISSUE:       2209
TITLE:       ISQL crashes when fetching data for a column having alias longer than 30 characters
DESCRIPTION:
JIRA:        CORE-1782
FBTEST:      bugs.core_1782
"""

import pytest
from firebird.qa import *

db = db_factory()

# version: 3.0

test_script_1 = """
    set list on;
    select 1 as test567890test567890test567890test567890 from rdb$database;
"""

act_1 = isql_act('db', test_script_1)

expected_stderr_1 = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Name longer than database column size
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stderr = expected_stderr_1
    act_1.execute()
    assert act_1.clean_stderr == act_1.clean_expected_stderr

# version: 4.0

test_script_2 = """
    set list on;
select 'Check column title, ASCII, width = 256' as
i2345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890i2345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890i23456789012345678901234567890123456
from rdb$database;
"""

act_2 = isql_act('db', test_script_2,
                 substitutions=[('-At line[:]{0,1}[\\s]+[\\d]+,[\\s]+column[:]{0,1}[\\s]+[\\d]+', '')])

expected_stderr_2 = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -token size exceeds limit
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stderr = expected_stderr_2
    act_2.execute()
    assert act_2.clean_stderr == act_2.clean_expected_stderr

