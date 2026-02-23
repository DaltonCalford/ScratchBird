#coding:utf-8

"""
ID:          issue-1748
ISSUE:       1748
TITLE:       Bug with size of alias name in a table (but still minor that 31 characters)
DESCRIPTION:
JIRA:        CORE-1329
FBTEST:      bugs.core_1329
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """CREATE TABLE BIG_TABLE_1234567890123 (COD INTEGER NOT NULL PRIMARY KEY);
COMMIT;
SELECT
BIG_TABLE_1234567890123.COD
FROM
BIG_TABLE_1234567890123
JOIN (SELECT
      BIG_TABLE_1234567890123.COD
      FROM
      BIG_TABLE_1234567890123) BIG_TABLE_1234567890123_ ON
BIG_TABLE_1234567890123.COD = BIG_TABLE_1234567890123_.COD;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
