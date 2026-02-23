#coding:utf-8

"""
ID:          issue-1904
ISSUE:       1904
TITLE:       DATEADD wrong work with NULL arguments
DESCRIPTION:
JIRA:        CORE-1489
FBTEST:      bugs.core_1489
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SELECT 1, DATEADD(SECOND, Null, CAST('01.01.2007' AS DATE)) FROM RDB$DATABASE;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CONSTANT     DATEADD
============ ===========
           1      <null>

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

