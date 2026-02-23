#coding:utf-8

"""
ID:          create-database-11
TITLE:       Create database: Default char set NONE
DESCRIPTION: This test should be implemented for all char sets.
FBTEST:      functional.database.create.11
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', 'SELECT RDB$CHARACTER_SET_NAME FROM rdb$Database;', substitutions=[('=.*', '')])

expected_stdout = """
    RDB$CHARACTER_SET_NAME
    NONE
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
