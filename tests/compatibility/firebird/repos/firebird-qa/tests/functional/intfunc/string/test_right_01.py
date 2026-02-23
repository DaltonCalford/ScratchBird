#coding:utf-8

"""
ID:          intfunc.string.right
TITLE:       RIGHT( <string>, <number> )
DESCRIPTION:
  Returns the substring, of the specified length, from the right-hand end of a string.
FBTEST:      functional.intfunc.string.right_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select RIGHT('NORD PAS DE CALAIS', 13) from rdb$database;")

expected_stdout = """
RIGHT
==================
PAS DE CALAIS
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
