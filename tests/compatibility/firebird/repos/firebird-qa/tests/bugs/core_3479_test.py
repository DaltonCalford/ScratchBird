#coding:utf-8

"""
ID:          issue-3839
ISSUE:       3839
TITLE:       ASCII_VAL raises error instead of return 0 for empty strings
DESCRIPTION: Added two expressions with "non-typical" arguments
JIRA:        CORE-3479
FBTEST:      bugs.core_3479
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select ascii_val('') v1, ascii_val(ascii_char(0)) v2, ascii_val(ascii_char(null)) v3 from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    V1                              0
    V2                              0
    V3                              <null>
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

