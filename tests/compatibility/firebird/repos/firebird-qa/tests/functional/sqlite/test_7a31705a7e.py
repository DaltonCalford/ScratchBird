#coding:utf-8

"""
ID:          7a31705a7e
ISSUE:       https://www.sqlite.org/src/tktview/7a31705a7e
TITLE:       Name resolution fails when table name is a prefix of another table
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
    create table t1 (a integer primary key);
    create table t2 (a integer primary key, b integer);
    create table t2x (b integer primary key);

    set count on;
    select * from (select x.b from (select t1.a as b from t1 join t2 on t1.a=t2.a) as x join t2x on x.b=t2x.b) y;
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
