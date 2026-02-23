#coding:utf-8

"""
ID:          issue-2074
ISSUE:       2074
TITLE:       Infinite row generation in "select gen_id(..) from rdb$database" with "group by"
DESCRIPTION:
JIRA:        CORE-1650
FBTEST:      bugs.core_1650
"""

import pytest
from firebird.qa import *

init_script = """create generator g;
commit;
"""

db = db_factory(init=init_script)

test_script = """select first 10 1, gen_id(g, 1 )
from rdb$database
group by 1,2;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CONSTANT                GEN_ID
============ =====================
           1                     3

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

