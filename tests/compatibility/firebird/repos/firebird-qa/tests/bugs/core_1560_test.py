#coding:utf-8

"""
ID:          issue-1979
ISSUE:       1979
TITLE:       NULLIF crashes when first parameter is constant empty string
DESCRIPTION:
JIRA:        CORE-1560
FBTEST:      bugs.core_1560
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select nullif('','') from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
CASE
======
<null>

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

