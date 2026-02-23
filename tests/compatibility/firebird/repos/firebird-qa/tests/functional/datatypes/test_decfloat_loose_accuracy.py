#coding:utf-8

"""
ID:          decfloat.preserve-accuracy
TITLE:       Test for preseving accuracy when evaluate sum of values with huge difference in magnitude
DESCRIPTION:
    Wide range of terms can lead to wrong result of sum.
    https://en.wikipedia.org/wiki/Decimal_floating_point
    https://en.wikipedia.org/wiki/Kahan_summation_algorithm
FBTEST:      functional.datatypes.decfloat_loose_accuracy
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
         1
        +cast(1e33 as decfloat)
        -cast(1e33 as decfloat)
        as addition_with_e33
    from rdb$database;

    select
         1
        +cast(1e34 as decfloat)
        -cast(1e34 as decfloat)
        as addition_with_e34
    from rdb$database;

"""

act = isql_act('db', test_script)

expected_stdout = """
    ADDITION_WITH_E33                                                        1
    ADDITION_WITH_E34                                                     0E+1
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
