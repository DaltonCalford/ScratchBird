#coding:utf-8

"""
ID:          intfunc.binary.shr
TITLE:       New Built-in Functions, Firebird 2.1 : BIN_SHR( <number>, <number> )
DESCRIPTION: Returns the result of a binary shift left operation performed on the arguments (first << second).
FBTEST:      functional.intfunc.binary.shr_01
"""

import pytest
from firebird.qa import *

db = db_factory()

act = isql_act('db', "select BIN_SHR(8, 1) from rdb$database;")

expected_stdout = """
              BIN_SHR
=====================
                    4
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
