#coding:utf-8

"""
ID:          issue-6596
ISSUE:       6596
TITLE:       TRUNC() does not accept second argument = -128 (but shows it as required boundary in error message)
DESCRIPTION:
JIRA:        CORE-6355
FBTEST:      bugs.core_6355
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select trunc(0,-128) from rdb$database;
    select trunc(9223372036854775807,-128) from rdb$database;
    select trunc(170141183460469231731687303715884105727,-128) from rdb$database;
    select trunc(-9223372036854775808,-128) from rdb$database;
    select trunc(-170141183460469231731687303715884105728,-128) from rdb$database;

    -- (optional) check upper bound (127):
    select trunc(0,127) from rdb$database;
    select trunc(9223372036854775807,127) from rdb$database;
    select trunc(170141183460469231731687303715884105727,127) from rdb$database;
    select trunc(-9223372036854775808,127) from rdb$database;
    select trunc(-170141183460469231731687303715884105728,127) from rdb$database;

"""

act = isql_act('db', test_script)

expected_stdout = """
    0
    0
    0
    0
    0
    0
    9223372036854775807
    170141183460469231731687303715884105727
    -9223372036854775808
    -170141183460469231731687303715884105728
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
