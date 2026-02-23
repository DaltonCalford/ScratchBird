#coding:utf-8

"""
ID:          issue-1937
ISSUE:       1937
TITLE:       Inconsistent DATEDIFF behaviour
DESCRIPTION:
JIRA:        CORE-1522
FBTEST:      bugs.core_1522
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SELECT DATEDIFF(HOUR, CAST('01:59:59' AS TIME), CAST('02:59:58' AS TIME)) FROM RDB$DATABASE;
SELECT DATEDIFF(HOUR, CAST('01:59:59' AS TIME), CAST('02:59:59' AS TIME)) FROM RDB$DATABASE;
"""

act = isql_act('db', test_script)

expected_stdout = """
             DATEDIFF
=====================
                    1


             DATEDIFF
=====================
                    1

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

