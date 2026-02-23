#coding:utf-8

"""
ID:          issue-2001
ISSUE:       2001
TITLE:       ABS() rounds NUMERIC values
DESCRIPTION:
JIRA:        CORE-1582
FBTEST:      bugs.core_1582
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SELECT
  ABS(CAST(-1.98 AS NUMERIC(10,2))),
  ABS(CAST(-1.23 AS DECIMAL(10,2))),
  ABS(CAST(-1.98 AS NUMERIC(9,2))),
  ABS(CAST(-1.23 AS DECIMAL(9,2)))
  FROM RDB$DATABASE;
"""

act = isql_act('db', test_script)

expected_stdout = """
                  ABS                   ABS                   ABS                   ABS
===================== ===================== ===================== =====================
                 1.98                  1.23                  1.98                  1.23

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

