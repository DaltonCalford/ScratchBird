#coding:utf-8

"""
ID:          f8a7060ece
ISSUE:       https://www.sqlite.org/src/tktview/f8a7060ece
TITLE:       Incorrect result for query that uses MIN() and a CAST on rowid
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c0 int unique, c1 int);
    insert into t0(c1) values (0);
    insert into t0(c0) values (0);
    create view v0(c0, c1) as 
    select t0.c1, t0.c0 
    from (select c0, c1, row_number()over(order by rdb$db_key) as rowid from t0) t0
    where cast(t0.rowid as int) = 1;

    set count on;
    select v0.c0, min(v0.c1)over() from v0;
    set count off;
    commit;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 0
    MIN <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
