#coding:utf-8

"""
ID:          issue-5370
ISSUE:       5370
TITLE:       Allow to fixup (nbackup) database via Services API
DESCRIPTION:
JIRA:        CORE-5085
FBTEST:      bugs.core_5085
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.nbackup(switches=['-l', str(act.db.db_path)])
    #with act.connect_server() as srv:
        # This raises error in new FB OO API while calling spb.insert_string(SPBItem.DBNAME, database):
        # "Internal error when using clumplet API: attempt to store data in dataless clumplet"
        #srv.database.nfix_database(database=act.db.db_path)
    # So we have to use svcmgr...
    act.reset()
    act.svcmgr(switches=['action_nfix', 'dbname', str(act.db.db_path)])
    with act.db.connect() as con:
        c = con.cursor()
        result = c.execute('select mon$backup_state from mon$database').fetchall()
    assert result == [(0, )]
