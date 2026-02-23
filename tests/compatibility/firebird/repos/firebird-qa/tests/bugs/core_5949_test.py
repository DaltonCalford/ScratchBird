#coding:utf-8

"""
ID:          issue-6205
ISSUE:       6205
TITLE:       Bugcheck could happen when read-only database with non-zero linger is set to read-write mode
DESCRIPTION:
JIRA:        CORE-5949
FBTEST:      bugs.core_5949
"""

import pytest
from firebird.qa import *
from firebird.driver import DbAccessMode

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3.0.5')
def test_1(act: Action):
    with act.db.connect() as con:
        con.execute_immediate('alter database set linger to 60')
        con.commit()
    #
    with act.connect_server() as srv:
        srv.database.set_access_mode(database=act.db.db_path, mode=DbAccessMode.READ_ONLY)
    # Test
    with act.db.connect() as con:
        c = con.cursor()
        c.execute('select r.rdb$linger, d.mon$read_only from rdb$database r cross join mon$database d')
        result = c.fetchone()
        con.commit()
    assert result == (60, 1)
