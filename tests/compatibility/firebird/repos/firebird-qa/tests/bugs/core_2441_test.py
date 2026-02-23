#coding:utf-8

"""
ID:          issue-1069
ISSUE:       1069
TITLE:       Server crashes on UPDATE OR INSERT statement
DESCRIPTION:
JIRA:        CORE-2441
FBTEST:      bugs.core_2441
"""

import pytest
import datetime
from firebird.qa import *

init_script = """CREATE TABLE TABLE_TXT (
    FIELD1 VARCHAR(255)
);
commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        c.execute("""UPDATE OR INSERT INTO TABLE_TXT (FIELD1)
        VALUES (CAST(? AS TIMESTAMP))
        MATCHING(FIELD1)""", [datetime.datetime(2011,5,1)])
