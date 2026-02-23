#coding:utf-8

"""
ID:          issue-6211
ISSUE:       6211
TITLE:       Bug in SIMILAR TO when adding numeric quantifier as bound for repetetion
  of expression leads to empty resultset
DESCRIPTION:
JIRA:        CORE-5957
FBTEST:      bugs.core_5957
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    set count on;
    select 1 from rdb$database where 'SLEEP' similar to '(DELAY|SLEEP|PAUSE){1}'; -- 2.5 fails here
    select 2 from rdb$database where 'SLEEP' similar to '(DELAY|SLEEP|PAUSE){1,}';  -- 2.5 fails here
    select 3 from rdb$database where 'SLEEP' similar to '(DELAY|SLEEP|PAUSE)+';
    select 4 from rdb$database where 'SLEEP' similar to '(DELAY|SLEEP|PAUSE)*';
"""

act = isql_act('db', test_script)

expected_stdout = """
    1
    Records affected: 1

    2
    Records affected: 1

    3
    Records affected: 1

    4
    Records affected: 1

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
