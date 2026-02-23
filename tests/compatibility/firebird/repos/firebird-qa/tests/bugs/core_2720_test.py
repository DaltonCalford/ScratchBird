#coding:utf-8

"""
ID:          issue-3116
ISSUE:       3116
TITLE:       Wrong evaluation result with divide and unary plus\\minus operations
DESCRIPTION:
JIRA:        CORE-2720
FBTEST:      bugs.core_2720
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
        36/4/3    div_01
       ,36/-4/3   div_02
       ,36/+4/3   div_03
       ,36/+-4/3  div_04
       ,36/-+4/3  div_05
       ,36/- -4/3 div_06
       ,36/++4/3  div_07
       ,- -36/- -4/- -3 div_08
       ,- -36/- -3/- -4 div_09
       ,- +36/+ -3/+ -4 div_10
    from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    DIV_01 3
    DIV_02 -3
    DIV_03 3
    DIV_04 -3
    DIV_05 -3
    DIV_06 3
    DIV_07 3
    DIV_08 3
    DIV_09 3
    DIV_10 -3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

