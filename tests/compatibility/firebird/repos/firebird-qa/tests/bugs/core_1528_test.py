#coding:utf-8

"""
ID:          issue-1942
ISSUE:       1942
TITLE:       Functions DATEDIFF does not work in dialect 1
DESCRIPTION:
JIRA:        CORE-1528
FBTEST:      bugs.core_1528
"""

import pytest
from firebird.qa import *

db = db_factory(sql_dialect=1)

test_script = """SELECT DATEDIFF(DAY, CAST('18.10.2007' AS TIMESTAMP), CAST('23.10.2007' AS TIMESTAMP)) FROM RDB$DATABASE;"""

act = isql_act('db', test_script)

expected_stdout = """
               DATEDIFF
=======================
      5.000000000000000
"""

@pytest.mark.version('>=2.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

