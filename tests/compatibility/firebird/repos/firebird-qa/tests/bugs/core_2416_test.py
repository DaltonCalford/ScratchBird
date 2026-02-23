#coding:utf-8

"""
ID:          issue-2834
ISSUE:       2834
TITLE:       AV preparing a query with aggregate over derived table
DESCRIPTION:
JIRA:        CORE-2416
FBTEST:      bugs.core_2416
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """WITH
  t0 AS (
    SELECT 1 AS f0, date '01.03.09' AS f1, 'Event1' AS f2
      FROM rdb$database
  ),

  t1 (f1) AS (
    SELECT MIN(t2.f1) AS f1 FROM t0 AS t2 WHERE t2.f0 > t3.f0 AND t2.f1 >= t3.f1 AND t2.f2 = t3.f2
  )

SELECT t4.f2, t4.f1_p
  FROM (SELECT t3.f0, t3.f1, t3.f2, CAST((SELECT t1.f1 FROM t1) - t3.f1 AS INTEGER) AS f1_p
          FROM t0 AS t3
       ) AS t4
  WHERE t4.f1_p IS NOT NULL
GROUP BY t4.f2, t4.f1_p;

SELECT t4.f2, t4.f1_p
  FROM (SELECT t3.f0, t3.f1, t3.f2,
               CAST((SELECT t1.f1 FROM (
                            SELECT MIN(t2.f1) AS f1 FROM (
                                   SELECT 1 AS f0, date '01.03.09' AS f1, 'Event1' AS f2
                                     FROM rdb$database) AS t2
                             WHERE t2.f0 > t3.f0 AND t2.f1 >= t3.f1 AND t2.f2 = t3.f2) as t1)
                    - t3.f1 AS INTEGER) AS f1_p
          FROM (
            SELECT 1 AS f0, date '01.03.09' AS f1, 'Event1' AS f2
              FROM rdb$database) AS t3
       ) AS t4
  WHERE t4.f1_p IS NOT NULL
GROUP BY t4.f2, t4.f1_p ;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
