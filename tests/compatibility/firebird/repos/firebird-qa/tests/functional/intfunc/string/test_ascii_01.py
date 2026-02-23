#coding:utf-8

"""
ID:          intfunc.string.ascii
TITLE:       ASCII_CHAR( <number> )
DESCRIPTION:
  Returns the ASCII character with the specified code. The argument to ASCII_CHAR must be
  in the range 0 to 255. The result is returned in character set NONE.
FBTEST:      functional.intfunc.string.ascii_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select ASCII_CHAR( 065 ) from rdb$database;")

expected_stdout = """
ASCII_CHAR
==========
A
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
