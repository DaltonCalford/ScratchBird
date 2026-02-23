#coding:utf-8

"""
ID:          issue-1921
ISSUE:       1921
TITLE:       Server crash with isc_dsql_execute_immediate and zero length string
DESCRIPTION:
JIRA:        CORE-1506
FBTEST:      bugs.core_1506
"""

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()

act = python_act('db', substitutions=[('- SQL error code.*', '')])

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.db.connect() as con:
        with pytest.raises(DatabaseError, match='.*-Unexpected end of command.*'):
            con.execute_immediate('')


