#coding:utf-8

"""
ID:          31338dca7e
ISSUE:       https://www.sqlite.org/src/tktview/31338dca7e
TITLE:       OR operator in WHERE clause gives wrong answer when indexed
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(x int);
    create table t2(y int);
    insert into t1 values(111);
    insert into t1 values(222);
    insert into t2 values(333);
    insert into t2 values(444);

    set count on;
    select 'noindex', t1.*, t2.* from t1, t2
    where (x=111 and y!=444) or x=222;
    commit;

    create index t1x on t1(x);

    select 'w/index', t1.*, t2.* from t1, t2
    where (x=111 and y!=444) or x=222;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    CONSTANT noindex
    X 111
    Y 333

    CONSTANT noindex
    X 222
    Y 333
    
    CONSTANT noindex
    X 222
    Y 444
    
    Records affected: 3
    
    CONSTANT w/index
    X 111
    Y 333
    
    CONSTANT w/index
    X 222
    Y 333
    
    CONSTANT w/index
    X 222
    Y 444
    
    Records affected: 3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
