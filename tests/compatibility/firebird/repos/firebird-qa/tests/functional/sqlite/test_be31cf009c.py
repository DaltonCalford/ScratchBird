#coding:utf-8

"""
ID:          be31cf009c
ISSUE:       https://www.sqlite.org/src/tktview/be31cf009c
TITLE:       Unexpected result for % and '1E1'
DESCRIPTION:
NOTES:
    [14.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select mod(1, -1e0) as "mod(1, -1e0)" from rdb$database;
    select mod(1, -1e1) as "mod(1, -1e1)" from rdb$database;
    select mod(1,  1e0) as "mod(1,  1e0)" from rdb$database;
    select mod(1,  1e1) as "mod(1,  1e1)" from rdb$database;
    select mod(1, '-1e0') as "mod(1, '-1e0')" from rdb$database;
    select mod(1, '-1e1') as "mod(1, '-1e1')" from rdb$database;
    select mod(1,  '1e0') as "mod(1,  '1e0')" from rdb$database;
    select mod(1,  '1e1') as "mod(1,  '1e1')" from rdb$database;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    mod(1, -1e0)   0
    mod(1, -1e1)   1
    mod(1, 1e0)    0
    mod(1, 1e1)    1
    mod(1, '-1e0') 0
    mod(1, '-1e1') 1
    mod(1, '1e0')  0
    mod(1, '1e1')  1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
