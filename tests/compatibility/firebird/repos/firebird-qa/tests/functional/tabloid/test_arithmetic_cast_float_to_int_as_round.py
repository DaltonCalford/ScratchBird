#coding:utf-8

"""
ID:          tabloid.arithmetic-cast-float-to-int-as-round
TITLE:       Result of CAST for numbers is implementation defined
DESCRIPTION: 
  See also: sql.ru/forum/actualutils.aspx?action=gotomsg&tid=1062610&msg=15214333
FBTEST:      functional.tabloid.arithmetic_cast_float_to_int_as_round
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on; select cast( sqrt(24) as smallint) casted_sqrt from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CASTED_SQRT                     5
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
