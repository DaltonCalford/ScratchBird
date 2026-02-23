#coding:utf-8

"""
ID:          issue-6597
ISSUE:       6597
TITLE:       ROUND() does not allow second argument >=1 when its first argument is more than MAX_BIGINT / 10
DESCRIPTION:
JIRA:        CORE-6356
FBTEST:      bugs.core_6356
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select round( 9223372036854775807, 1) from rdb$database;
    select round( 170141183460469231731687303715884105727, 1) from rdb$database;


    select round( -9223372036854775808, 1) from rdb$database;
    select round( -170141183460469231731687303715884105728, 1) from rdb$database;

    select round( 9223372036854775807, 127) from rdb$database;
    select round( 170141183460469231731687303715884105727, 127) from rdb$database;

    select round( -9223372036854775808, -128) from rdb$database;
    select round( -170141183460469231731687303715884105728, -128) from rdb$database;

    select round( -9223372036854775808, 127) from rdb$database;
    select round( -170141183460469231731687303715884105728, 127) from rdb$database;

"""

act = isql_act('db', test_script)

expected_stdout = """
    9223372036854775807
    170141183460469231731687303715884105727
    -9223372036854775808
    -170141183460469231731687303715884105728
    9223372036854775807
    170141183460469231731687303715884105727
    0
    0
    -9223372036854775808
    -170141183460469231731687303715884105728
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
