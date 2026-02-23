#coding:utf-8

"""
ID:          intfunc.string.trunc
TITLE:       Test for TRUNC function
DESCRIPTION:
    TRUNC( <number> [, <number> ] )
    Returns the integral part (up to the specified scale) of a number.
FBTEST:      functional.intfunc.string.trunc_01
"""

import os
import platform
import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    select trunc(-2.8), trunc(2.8)
        from rdb$database;  -- returns -2, 2
    select trunc(987.65, 1), trunc(987.65, -1)
       from rdb$database;  -- returns 987.60, 980.00
"""

act = isql_act('db', test_script)

expected_stdout = """
                TRUNC                 TRUNC
===================== =====================
                   -2                     2

                TRUNC                 TRUNC
===================== =====================
               987.60                980.00
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
