#coding:utf-8

"""
ID:          intfunc.string.reverse
TITLE:       REVERSE( <value> )
DESCRIPTION:
  Returns a string in reverse order. Useful function for creating an expression index that
  indexes strings from right to left.
FBTEST:      functional.intfunc.string.reverse_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select REVERSE('DRON') from rdb$database;")

expected_stdout = """
REVERSE
=======
NORD
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
