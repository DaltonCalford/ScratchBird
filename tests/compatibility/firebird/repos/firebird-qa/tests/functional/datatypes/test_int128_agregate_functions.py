#coding:utf-8

"""
ID:          int128.agregate-functions
ISSUE:       6585
JIRA:        CORE-6344
TITLE:       Basic test for aggregation functions against INT128 datatype
DESCRIPTION:
    Test verifies https://github.com/FirebirdSQL/firebird/commit/17b287f9ce882e27de76c416175ad0453520528e
    (Postfix for CORE-6344 - fixed SUM() & AVG()).
FBTEST:      functional.datatypes.int128-agregate-functions
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table test(i128_least int128 , i128_great int128);
    insert into test(i128_least, i128_great) values(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727);

    select sum(i128_least) as agg_sum_a from test;
    select sum(i128_great) as agg_sum_b from test;

    select avg(i128_least) as agg_avg_a from test;
    select avg(i128_great) as agg_avg_b from test;

    select min(i128_least) as agg_min_a from test;
    select min(i128_great) as agg_min_b from test;

    select max(i128_least) as agg_max_a from test;
    select max(i128_great) as agg_max_b from test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    AGG_SUM_A                            -170141183460469231731687303715884105728
    AGG_SUM_B                             170141183460469231731687303715884105727
    AGG_AVG_A                            -170141183460469231731687303715884105728
    AGG_AVG_B                             170141183460469231731687303715884105727
    AGG_MIN_A                            -170141183460469231731687303715884105728
    AGG_MIN_B                             170141183460469231731687303715884105727
    AGG_MAX_A                            -170141183460469231731687303715884105728
    AGG_MAX_B                             170141183460469231731687303715884105727
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
