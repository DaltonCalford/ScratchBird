#coding:utf-8

"""
ID:          issue-820
ISSUE:       820
TITLE:       Redundant evaluations in COALESCE
DESCRIPTION:
JIRA:        CORE-474
FBTEST:      bugs.core_0474
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create generator g1;
    commit;
    set list on;
    select
        coalesce(
                   nullif(gen_id(g1,1),1),
                   nullif(gen_id(g1,1),2),
                   gen_id(g1,1),
                   nullif(gen_id(g1,1),4),
                   gen_id(g1,1)
                )
                as curr_gen
    from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CURR_GEN                        3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

