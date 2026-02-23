#coding:utf-8

"""
ID:          issue-7860
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7860
TITLE:       Crash potentially caused by BETWEEN Operator
DESCRIPTION:
NOTES:
    Confirmed bug on 6.0.0.132
    Checked on 6.0.0.150, 5.0.0.1278, 4.0.5.3031 (all - intermediate builds)
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    create table test(id int, primary key(id)); 
    select id from test where (1 not between false and id);
"""

expected_stdout = """
    Statement failed, SQLSTATE = 22018
    conversion error from string "1"
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.12')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
