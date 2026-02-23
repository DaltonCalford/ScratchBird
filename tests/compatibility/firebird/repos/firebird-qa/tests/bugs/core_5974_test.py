#coding:utf-8

"""
ID:          issue-6226
ISSUE:       6226
TITLE:       Wrong result of select distinct with decfload/timezone/collated column
DESCRIPTION:
JIRA:        CORE-5974
FBTEST:      bugs.core_5974
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test( d decfloat(34) );
    create index test_d on test(d);
    commit;

    insert into test select 15514 from rdb$types rows 3;
    commit;
    --set plan on;
    select distinct d+0 as d_distinct from test;
    select d+0 as d_grouped_nat from test group by d+0;
    select d as d_grouped_idx from test group by d;
"""

act = isql_act('db', test_script)

expected_stdout = """
    D_DISTINCT                                                           15514
    D_GROUPED_NAT                                                        15514
    D_GROUPED_IDX                                                        15514
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
