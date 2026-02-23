#coding:utf-8

"""
ID:          intfunc.binary.or
TITLE:       New Built-in Functions, Firebird 2.1 : BIN_OR( <number> [, <number> ...] )
DESCRIPTION: Returns the result of a binary OR operation performed on all arguments.
FBTEST:      functional.intfunc.binary.or_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select BIN_OR( 1, 1) from rdb$database;
select BIN_OR( 1, 0) from rdb$database;
select BIN_OR( 0, 0) from rdb$database;"""

act = isql_act('db', test_script)

expected_stdout = """
      BIN_OR
============
           1


      BIN_OR
============
           1


      BIN_OR
============
           0

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
