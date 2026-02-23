#coding:utf-8

"""
ID:          38cb5df375
ISSUE:       https://www.sqlite.org/src/tktview/38cb5df375
TITLE:       LIMIT ignored on compound query with subqueries
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
    create table t(a int);
    insert into t values(1);
    insert into t values(2);

    set count on;
    select * from (select * from t order by a)
    union all
    select * from (select a from t)
    rows 1;

"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    A 1
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
