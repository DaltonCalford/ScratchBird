#coding:utf-8

"""
ID:          intfunc.misc.hash
TITLE:       HASH( <value> )
DESCRIPTION: Returns a HASH of a value.
FBTEST:      functional.intfunc.misc.hash_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select hash('toto') from rdb$database;")

expected_stdout = """
HASH
=====================
505519
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
