#coding:utf-8

"""
ID:          issue-1679
ISSUE:       1679
TITLE:       String truncation error when concatenating _UTF8 string onto extract(year result
DESCRIPTION:
  SELECT ((EXTRACT(YEAR FROM CAST('2007-01-01' AS DATE)) || _UTF8'')) col FROM rdb$database GROUP BY 1;

  Produces the error
    Statement failed, SQLCODE = -802
    arithmetic exception, numeric overflow, or string truncation
JIRA:        CORE-1255
FBTEST:      bugs.core_1255
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SELECT ((EXTRACT(YEAR FROM CAST('2007-01-01' AS DATE)) || _UTF8'')) col FROM rdb$database GROUP BY 1;
"""

act = isql_act('db', test_script)

expected_stdout = """COL
======
2007

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

