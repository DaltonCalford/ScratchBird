#coding:utf-8

"""
ID:          issue-6756
ISSUE:       6756
TITLE:       Error "no current record for fetch operation" when sorting by a international string
DESCRIPTION:
JIRA:        CORE-6529
FBTEST:      bugs.core_6529
"""

import pytest
from firebird.qa import *

init_script = """
recreate table t (f varchar(32765) character set win1251) ;
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    with act.db.connect() as con:
        c = con.cursor()
        c.execute("insert into t(f) values(?)", ['W' * 1000])
        # no commit here!
        c.execute('select f from t order by 1')
        c.fetchall()
    # Passed.
