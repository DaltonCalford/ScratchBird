#coding:utf-8

"""
ID:          issue-6118
ISSUE:       6118
TITLE:       Command 'REVOKE ALL ON ALL FROM <anyname>' lead server to crash
DESCRIPTION:
JIRA:        CORE-5858
FBTEST:      bugs.core_5858
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set wng off;
    set bail on;
    revoke all on all from any_name_here;
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.execute()
