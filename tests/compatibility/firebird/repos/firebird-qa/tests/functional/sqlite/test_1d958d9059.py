#coding:utf-8

"""
ID:          1d958d9059
ISSUE:       https://www.sqlite.org/src/tktview/1d958d9059
TITLE:       Incorrect result with NOT IN operator and partial index
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int, b int);
    insert into t1 values(1,1);
    insert into t1 values(2,2);

    create table t2(x int);
    insert into t2 values(1);
    insert into t2 values(2);

    set count on;
    select 'one' msg, t2.* from t2 where x not in (select a from t1);
    set count off;
    commit;

    create index t1a on t1 computed by(a) where b=1;

    set count on;
    select 'two' msg, t2.* from t2 where x not in (select a from t1);
    set count off;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
    Records affected: 0
"""

@pytest.mark.version('>=5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
