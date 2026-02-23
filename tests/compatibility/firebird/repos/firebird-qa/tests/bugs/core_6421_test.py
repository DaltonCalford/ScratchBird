#coding:utf-8

"""
ID:          issue-6659
ISSUE:       6659
TITLE:       Parameter in offset expression in LAG, LEAD, NTH_VALUE window functions requires explicit cast to BIGINT or INTEGER
DESCRIPTION:
JIRA:        CORE-6421
FBTEST:      bugs.core_6421
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set planonly;

    select rdb$relation_name, lag(rdb$relation_name, ?) over (order by rdb$relation_name) from rdb$relations;
    select rdb$relation_name, lead(rdb$relation_name, ?) over (order by rdb$relation_name) from rdb$relations;
    select rdb$relation_name, nth_value(rdb$relation_name, ?) over (order by rdb$relation_name) from rdb$relations;
"""

act = isql_act('db', test_script, substitutions=[('PLAN .*', '')])

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.execute()
