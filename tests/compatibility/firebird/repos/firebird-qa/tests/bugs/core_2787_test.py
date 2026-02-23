#coding:utf-8

"""
ID:          issue-3178
ISSUE:       3178
TITLE:       Make rdb$system_flag not null
DESCRIPTION:
JIRA:        CORE-2787
FBTEST:      bugs.core_2787
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set count on;
    set list on;

    select rf.rdb$relation_name as rel_name, rf.rdb$null_flag as nullable
    from rdb$relation_fields rf
    where
        upper(rf.rdb$field_name) = upper('rdb$system_flag')
        and rdb$null_flag = 1;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

