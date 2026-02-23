#coding:utf-8

"""
ID:          issue-7795
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7795
TITLE:       NOT IN <list> returns incorrect result if NULLs are present inside the value list
DESCRIPTION:
NOTES:
    [12.10.2023] pzotov
    Confirmed bug on 5.0.0.1219, 6.0.0.76.
    Checked on 5.0.0.1244 (intermediate build).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(id int);
    insert into test(id) select row_number()over() from rdb$types rows 10;
    commit;
    select count(*) cnt_1 from test where id not in (3, 4);
    select count(*) cnt_2 from test where id not in (3, 4, null);
"""
expected_stdout = """
    CNT_1                           8
    CNT_2                           0
"""
act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
