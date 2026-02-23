#coding:utf-8

"""
ID:          issue-1293
ISSUE:       1293
TITLE:       SUBSTRING with NULL offset or length don't return NULL
DESCRIPTION:
JIRA:        CORE-896
FBTEST:      bugs.core_0896
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """select substring('abc' from null) from rdb$database;
select substring('abc' from 2 for null) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """SUBSTRING
=========
<null>

SUBSTRING
=========
<null>

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

