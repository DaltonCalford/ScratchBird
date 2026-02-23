#coding:utf-8

"""
ID:          issue-1708
ISSUE:       1708
TITLE:       Avoid data retrieval if the WHERE clause always evaluates to FALSE [CORE1287]
NOTES:
    [18.02.2023] pzotov
    Confirmed problem on 5.0.0.733: generator will be increased.
    Checked on 5.0.0.736 - works fine.
    NB. Currently this test contains only trivial case for check.
    More complex examples, including recursive SQL, will be added later.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create sequence g;
    create view v_test as select max(i) as max_i from (select gen_id(g,1) as i from rdb$relations);

    set list on;
    select * from v_test where 1 = 0;
    select gen_id(g,0) g_current_1 from rdb$database;

    select * from v_test where false;
    select gen_id(g,0) g_current_2 from rdb$database;

    select * from v_test where exists(select 1 from rdb$database where 1 = 0);
    select gen_id(g,0) g_current_3 from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    G_CURRENT_1                     0
    G_CURRENT_2                     0
    G_CURRENT_3                     0
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
