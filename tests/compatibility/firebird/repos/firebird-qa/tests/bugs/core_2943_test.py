#coding:utf-8

"""
ID:          issue-3325
ISSUE:       3325
TITLE:       Parsing error recursive query with two recursive parts
DESCRIPTION:
JIRA:        CORE-2943
FBTEST:      bugs.core_2943
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    with recursive
    tree (NAME) as (
      select r.rdb$relation_name from rdb$relations r
      union all
      select r2.rdb$relation_name || tree.NAME from rdb$relations r2, tree
      where 1 = 0
    ),
    tree_2 as (
        select c.rdb$character_set_name from rdb$character_sets c
        union all
        select c2.rdb$character_set_name from rdb$character_sets c2, tree_2
        where 1 = 0
    )
    select * from tree, tree_2 where NAME=upper('RDB$PAGES') and RDB$CHARACTER_SET_NAME=upper('NONE');
"""

act = isql_act('db', test_script)

expected_stdout = """
    NAME                            RDB$PAGES
    RDB$CHARACTER_SET_NAME          NONE
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

