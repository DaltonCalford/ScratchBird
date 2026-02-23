#coding:utf-8

"""
ID:          5e3c886796
ISSUE:       https://www.sqlite.org/src/tktview/5e3c886796
TITLE:       Correlated subquery on the RHS of an IN operator causes output of excessive rows
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
    create table t1(a integer primary key);
    create table t2(b integer primary key);

    insert into t1(a) values(1);
    insert into t1(a) values(2);
    insert into t2(b) values(1);

    set count on;
    select a from t1 where a not in (select a from t2);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
