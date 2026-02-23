#coding:utf-8

"""
ID:          7f7f8026ed
ISSUE:       https://www.sqlite.org/src/tktview/7f7f8026ed
TITLE:       Segfault following heavy SAVEPOINT usage
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create table t1(x integer primary key, y char(10));

    insert into t1(x,y) 
    with recursive c(x) as (select 1 x from rdb$database union all select x+1 from c where x<250)
    select x*10, x || '*'  from c;
    savepoint p1;

    select count(*) as cnt_point_1a from t1;

    insert into t1(x,y)
    with recursive c(x) as (select 1 x from rdb$database union all select x+1 from c where x<250)
    select x*10+1, x || '*'  from c;

    rollback to p1;

    select count(*) as cnt_point_1b from t1;

    savepoint p2;

    insert into t1(x,y)
    with recursive c(x) as (select 1 x from rdb$database union all select x+1 from c where x<10)
    select x*10+2, x || '*' from c;

    rollback to p2;
    release savepoint p1 only;
    commit;
    select count(*) as cnt_final from t1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    CNT_POINT_1A 250
    CNT_POINT_1B 250
    CNT_FINAL 250
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
