#coding:utf-8

"""
ID:          issue-4442
ISSUE:       4442
TITLE:       SIMILAR TO does not work with x-prefixed literals
DESCRIPTION:
JIRA:        CORE-4114
FBTEST:      bugs.core_4114
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
      iif(' ' similar to '[[:WHITESPACE:]]', 'T', 'F') as f1,
      iif(_win1252 x'20' similar to '[[:WHITESPACE:]]', 'T', 'F') as f2,
      iif(_win1252 x'20' similar to '%', 'T', 'F') as f3
    from RDB$DATABASE ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    F1                              T
    F2                              T
    F3                              T
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

