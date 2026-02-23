#coding:utf-8

"""
ID:          issue-2360
ISSUE:       2360
TITLE:       MON$DATABASE returns outdated transaction counters
DESCRIPTION:
  Fields MON$NEXT_TRANSACTION etc contain incorrect (outdated) numbers on Classic if
  there are other active attachments.
JIRA:        CORE-1926
FBTEST:      bugs.core_1926
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        c.execute('SELECT 1 FROM RDB$DATABASE')
        with act.db.connect() as con_detail:
            con_detail.begin()
            c_detail = con_detail.cursor()
            c_detail.execute("select MON$NEXT_TRANSACTION from MON$DATABASE")
            tra_1 = c_detail.fetchone()[0]
            con_detail.commit()
            c_detail.execute("select MON$NEXT_TRANSACTION from MON$DATABASE")
            tra_2 = c_detail.fetchone()[0]
            con_detail.commit()
    assert tra_2 - tra_1 == 1
