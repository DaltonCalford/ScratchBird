#coding:utf-8

"""
ID:          issue-1762
ISSUE:       1762
TITLE:       Bug with a simple case and a subquery
DESCRIPTION:
JIRA:        CORE-1343
FBTEST:      bugs.core_1343
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """--works fine (searched case with a subquery)

SELECT
  CASE
    WHEN (SELECT 'A' FROM RDB$DATABASE) = 'A' THEN
      'Y'
    WHEN (SELECT 'A' FROM RDB$DATABASE) = 'B' THEN
      'B'
    ELSE
      'N'
  END
FROM RDB$DATABASE;

--works fine (simple case without a subquery)
SELECT
  CASE 'A'
    WHEN 'A' THEN
      'Y'
    WHEN 'B' THEN
      'N'
    ELSE
      'U'
    END
FROM RDB$DATABASE;

--don't work (simple case with a subquery)
SELECT
  CASE (SELECT 'A' FROM RDB$DATABASE)
    WHEN 'A' THEN
      'Y'
    WHEN 'B' THEN
      'N'
    ELSE
      'U'
   END
FROM RDB$DATABASE;

/*
Invalid token.
invalid request BLR at offset 110.
context already in use (BLR error).
*/
"""

act = isql_act('db', test_script)

expected_stdout = """
CASE
======
Y


CASE
======
Y


CASE
======
Y

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

