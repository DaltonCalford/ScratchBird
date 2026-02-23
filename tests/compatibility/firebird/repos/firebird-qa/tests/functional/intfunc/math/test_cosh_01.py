#coding:utf-8

"""
ID:          intfunc.math.cosh
TITLE:       COSH( <number>)
DESCRIPTION: Returns the hyperbolic cosine of a number.
FBTEST:      functional.intfunc.math.cosh_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select COSH( 1) from rdb$database;
select COSH( 0) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
                         COSH
      =======================
            1.543080634815244


                         COSH
      =======================
            1.000000000000000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
