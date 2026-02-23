#coding:utf-8

"""
ID:          issue-1804
ISSUE:       1804
TITLE:       Generated columns
DESCRIPTION:
JIRA:        CORE-1386
FBTEST:      bugs.core_1386
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table tab1 (col1 integer, col2 generated always as (col1 +1), col3 integer generated always as (col1 +1));
    commit;
    insert into tab1 (col1) values (1);
    commit;
    select * from tab1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COL1 1
    COL2 2
    COL3 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

