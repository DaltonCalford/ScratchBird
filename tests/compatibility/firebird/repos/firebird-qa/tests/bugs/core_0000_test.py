#coding:utf-8

"""
ID:          dummy
ISSUE:
TITLE:       Dummy test
DESCRIPTION:
JIRA:
FBTEST:      bugs.core_0000
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t1(id int);
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
