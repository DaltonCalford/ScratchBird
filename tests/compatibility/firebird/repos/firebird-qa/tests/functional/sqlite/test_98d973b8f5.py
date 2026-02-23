#coding:utf-8

"""
ID:          98d973b8f5
ISSUE:       https://www.sqlite.org/src/tktview/98d973b8f5
TITLE:       Partial index gives incorrect query result
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int, b char(9));
    create table t2(c char(9), d char(9));
    insert into t1 values(1, 'xyz');
    insert into t2 values('abc', 'not xyz');
    commit;
    create index i2 on t2(c) where d='xyz'; 
    set count on;
    select * from (select * from t1 where a=1 and b='xyz'), t2 where c='abc';
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 1
    B xyz
    C abc
    D not xyz
    Records affected: 1
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
