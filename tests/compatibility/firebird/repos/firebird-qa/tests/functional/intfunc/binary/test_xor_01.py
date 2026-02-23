#coding:utf-8

"""
ID:          intfunc.binary.xor
TITLE:       New Built-in Functions, Firebird 2.1 : BIN_XOR( <number> [, <number> ...] )
DESCRIPTION: Returns the result of a binary XOR operation performed on all arguments.
FBTEST:      functional.intfunc.binary.xor_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select BIN_XOR( 0,1) from rdb$database;
select BIN_XOR( 0,0) from rdb$database;
select BIN_XOR( 1,1) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
     BIN_XOR
============
           1


     BIN_XOR
============
           0


     BIN_XOR
============
           0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
