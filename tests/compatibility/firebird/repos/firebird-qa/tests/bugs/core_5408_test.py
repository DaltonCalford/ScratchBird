#coding:utf-8

"""
ID:          issue-5681
ISSUE:       5681
TITLE:       Result of boolean expression can not be concatenated with string literal
DESCRIPTION:
JIRA:        CORE-5408
FBTEST:      bugs.core_5408
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select (true = true)|| 'aaa' as "(true=true) concat 'aaa'" from rdb$database;
    select (true is true)|| 'aaa' as "(true is true) concat 'aaa'" from rdb$database;
    select 'aaa' || (true = true) as "'aaa' concat (true = true)" from rdb$database;
    select 'aaa' || (true is true) as "'aaa' concat (true is true)" from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    (true=true) concat 'aaa'        TRUEaaa
    (true is true) concat 'aaa'     TRUEaaa
    'aaa' concat (true = true)      aaaTRUE
    'aaa' concat (true is true)     aaaTRUE
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

