#coding:utf-8

"""
ID:          intfunc.list-03
ISSUE:       1367
TITLE:       List function with distinct option
DESCRIPTION:
JIRA:        CORE-964
FBTEST:      functional.intfunc.list.03
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
SELECT RDB$SYSTEM_FLAG, LIST(DISTINCT TRIM(RDB$OWNER_NAME)) FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG=1 GROUP BY 1;
"""

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        with con.cursor() as c:
            c.execute("SELECT RDB$SYSTEM_FLAG, LIST(DISTINCT TRIM(RDB$OWNER_NAME)) FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG=1 GROUP BY 1")
            result = c.fetchall()
    assert result == [(1, 'SYSDBA')]
