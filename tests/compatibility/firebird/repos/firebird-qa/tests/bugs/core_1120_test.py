#coding:utf-8

"""
ID:          issue-1541
ISSUE:       1541
TITLE:       Conversion from string to number is not standard compliant
DESCRIPTION:
JIRA:        CORE-1120
FBTEST:      bugs.core_1120
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select cast(5.6 as integer) from rdb$database;
select cast('5.6' as integer) from rdb$database;
select cast('5,6' as integer) from rdb$database;
select cast('5,6,7 8 9' as integer) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """CAST
============
           6

CAST
============
           6

CAST
============
CAST
============
"""

expected_stderr = """Statement failed, SQLSTATE = 22018
conversion error from string "5,6"
Statement failed, SQLSTATE = 22018
conversion error from string "5,6,7 8 9"
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

