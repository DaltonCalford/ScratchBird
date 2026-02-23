#coding:utf-8

"""
ID:          tabloid.arithmetic-numexpr-eval-dialect-3
TITLE:       Check result of integer division on dialect 3.
DESCRIPTION: 
  Was fixed in 2.1, see: sql.ru/forum/actualutils.aspx?action=gotomsg&tid=708324&msg=7865013
FBTEST:      functional.tabloid.arithmetic_numexpr_eval_dialect_3
"""

import pytest
from firebird.qa import *

db = db_factory(page_size=4096)

test_script = """
    set list on; select 36/-4/3 d from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    D                               -3
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
