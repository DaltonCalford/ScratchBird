#coding:utf-8

"""
ID:          intfunc.binary.and
TITLE:       New Built-in Functions, Firebird 2.1 : BIN_AND( <number> [, <number> ...] )
DESCRIPTION: Returns the result of a binary AND operation performed on all arguments.
FBTEST:      functional.intfunc.binary.and_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select BIN_AND( 1, 1) from rdb$database;
select BIN_AND( 1, 0) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
     BIN_AND
============
           1


     BIN_AND
============
           0

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
