#coding:utf-8

"""
ID:          issue-4162
ISSUE:       4162
TITLE:       RDB$TYPES contain duplicate character sets
DESCRIPTION:
JIRA:        CORE-3820
FBTEST:      bugs.core_3820
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    select rdb$field_name, rdb$type_name, count(rdb$type_name)
    from rdb$types
    group by rdb$field_name, rdb$type_name
    having count(rdb$type_name) > 1;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
