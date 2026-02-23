#coding:utf-8

"""
ID:          issue-3217
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/3217
TITLE:       isql shouldn't display db and user name when extracting a script
DESCRIPTION:
JIRA:        CORE-2831
FBTEST:      bugs.core_2831
NOTES:
    [10.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('^((?!(SQLSTATE|Database:|User:)).)*$', '')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = ""
    act.isql(switches=['-x'], combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
