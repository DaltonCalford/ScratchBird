#coding:utf-8

"""
ID:          issue-4551
ISSUE:       4551
TITLE:       Regression: Wrong evaluation of BETWEEN and boolean expressions due to parser conflict
DESCRIPTION:
JIRA:        CORE-4227
FBTEST:      bugs.core_4227
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select 1 x from rdb$database where rdb$relation_id between 1 and 500 and rdb$description is null;
"""

act = isql_act('db', test_script)

expected_stdout = """
    X                               1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
