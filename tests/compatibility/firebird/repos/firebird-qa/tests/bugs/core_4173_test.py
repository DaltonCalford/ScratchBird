#coding:utf-8

"""
ID:          issue-4499
ISSUE:       4499
TITLE:       Setting generator value twice in single transaction will set it to zero
DESCRIPTION:
JIRA:        CORE-4173
FBTEST:      bugs.core_4173
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set autoddl off;
    set list on;
    create generator g;
    commit;
    set generator g to 111;
    commit;
    select gen_id(g,0) value_on_step_1 from rdb$database;
    set generator g to 222;
    set generator g to 333;
    select gen_id(g,0) value_on_step_2 from rdb$database;
    commit;
    select gen_id(g,0) value_on_step_3 from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    VALUE_ON_STEP_1                 111
    VALUE_ON_STEP_2                 333
    VALUE_ON_STEP_3                 333
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
