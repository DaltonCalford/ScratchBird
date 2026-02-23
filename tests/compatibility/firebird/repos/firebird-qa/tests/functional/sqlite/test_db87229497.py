#coding:utf-8

"""
ID:          db87229497
ISSUE:       https://www.sqlite.org/src/tktview/db87229497
TITLE:       Incorrect result when RHS of IN operator contains DISTINCT and LIMIT
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
    create table t1(a int);
    create table t2(b int);
    insert into t1 values(1);
    insert into t1 values(1);
    insert into t1 values(2);
    insert into t2 values(1);
    insert into t2 values(2);
    set count on;
    select * from t2 where b in (select distinct a from t1 rows 2);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    B 1
    B 2
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
