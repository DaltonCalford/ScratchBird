#coding:utf-8

"""
ID:          issue-2983
ISSUE:       2983
TITLE:       The server crashes when selecting from the MON$ tables in the ON DISCONNECT trigger
DESCRIPTION:
JIRA:        CORE-2573
FBTEST:      bugs.core_2573
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    sql = """CREATE OR ALTER TRIGGER DIST
    ON DISCONNECT POSITION 0
    AS
    declare variable i integer;
    begin
      select mon$stat_id from mon$attachments rows 1 into :i;
    end
    """
    with act.db.connect() as con:
        c = con.cursor()
        c.execute(sql)
        con.commit()
    # The server should could be crashed at this point
    with act.db.connect() as con:
        c = con.cursor()
        c.execute('DROP TRIGGER DIST')
        con.commit()


