#coding:utf-8

"""
ID:          issue-3174
ISSUE:       3174
TITLE:       AV using recursive query as subquery in SELECT list and ORDER'ing by them
DESCRIPTION:
JIRA:        CORE-2783
FBTEST:      bugs.core_2783
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """SELECT RDB$RELATION_ID,
       (WITH RECURSIVE
         NUM (ID) AS
         (
           SELECT 1 FROM RDB$DATABASE

           UNION ALL

           SELECT ID + 1
             FROM NUM
            WHERE ID < 10
         )
        SELECT MAX(ID) FROM NUM
       ) AS NNN
  FROM RDB$DATABASE
ORDER BY NNN;
WITH RECURSIVE
  NUM (ID) AS
  (
           SELECT 1 FROM RDB$DATABASE

           UNION ALL

           SELECT ID + 1
             FROM NUM
            WHERE ID < 10
  )
SELECT RDB$RELATION_ID, (SELECT MAX(ID) FROM NUM) AS NNN
  FROM RDB$DATABASE
ORDER BY NNN;
"""

act = isql_act('db', test_script)

expected_stdout = """
RDB$RELATION_ID          NNN
=============== ============
            128           10

RDB$RELATION_ID          NNN
=============== ============
            128           10
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

