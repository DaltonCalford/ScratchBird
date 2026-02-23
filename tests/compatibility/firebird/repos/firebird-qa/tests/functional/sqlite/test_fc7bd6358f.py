#coding:utf-8

"""
ID:          fc7bd6358f
ISSUE:       https://www.sqlite.org/src/tktview/fc7bd6358f
TITLE:       Incorrect query result in a 3-way join due to affinity issues
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
    create table t(textid char(2));
    create table i(intid integer primary key);

    insert into t values('12');
    insert into t values('34');
    insert into i values(12);
    insert into i values(34);

    set count on;
    select t1.textid as a, i.intid as b, t2.textid as c
    from t t1, i, t t2
    where t1.textid = i.intid and t1.textid = t2.textid;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 12
    B 12
    C 12
    A 34
    B 34
    C 34
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
