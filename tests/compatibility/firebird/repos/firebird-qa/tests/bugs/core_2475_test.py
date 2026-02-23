#coding:utf-8

"""
ID:          issue-2888
ISSUE:       2888
TITLE:       External table data not visible to other sessions in Classic
DESCRIPTION:
  In 2.1.2 SuperServer, any data written to external tables are visible to other sessions.
  However in Classic, this data is not visible. It seems to be cached and written to file
  eventually, when this happens it becomes visible.
NOTES:
  THIS TEST WILL END WITH ERROR IF EXTERNAL TABLE ACCESS IS NOT ALLOWED, WHICH IS BY DEFAULT.
  It's necessary to adjust firebird.conf.
JIRA:        CORE-2475
FBTEST:      bugs.core_2475
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

external_table = temp_file('EXT1.TBL')

@pytest.mark.version('>=2.1.3')
def test_1(act: Action, external_table: Path):
    # Create external table
    act.isql(switches=[],
               input=f"create table EXT1 external file '{str(external_table)}' (PK INTEGER); exit;")
    # session A
    with act.db.connect() as con:
        c = con.cursor()
        c.execute("insert into EXT1 (PK) values (1)")
        con.commit()
    # session B
    with act.db.connect() as con:
        c = con.cursor()
        c.execute('select * from EXT1')
        result = c.fetchall()
    assert result == [(1, )]
