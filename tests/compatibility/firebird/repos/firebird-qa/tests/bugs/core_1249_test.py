#coding:utf-8

"""
ID:          issue-1673
ISSUE:       1673
TITLE:       Full shutdown mode doesn't work on Classic if there are other connections to the database
DESCRIPTION:
JIRA:        CORE-1249
FBTEST:      bugs.core_1249
"""

import pytest
from firebird.qa import *
from firebird.driver import ShutdownMode, ShutdownMethod, DatabaseError

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.connect_server() as srv, act.db.connect() as con:
        srv.database.shutdown(database=act.db.db_path, mode=ShutdownMode.FULL,
                              method=ShutdownMethod.FORCED, timeout=0)
        c = con.cursor()
        with pytest.raises(DatabaseError, match='.*shutdown'):
            c.execute('select 1 from rdb$database')
        #
        srv.database.bring_online(database=act.db.db_path)
