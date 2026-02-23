#coding:utf-8

"""
ID:          issue-2225
ISSUE:       2225
TITLE:       DB$DB_KEY evaluates to NULL in INSERT ... RETURNING
DESCRIPTION:
JIRA:        CORE-1798
FBTEST:      bugs.core_1798
"""

import pytest
from firebird.qa import *

init_script = """create table t (n integer);
"""

db = db_factory(init=init_script)

test_script = """insert into t values (1) returning rdb$db_key;
"""

act = isql_act('db', test_script)

expected_stdout = """
DB_KEY
================
8000000001000000

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

