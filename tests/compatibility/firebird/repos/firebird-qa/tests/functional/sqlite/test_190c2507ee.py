#coding:utf-8

"""
ID:          190c2507ee
ISSUE:       https://www.sqlite.org/src/tktview/190c2507ee
TITLE:       Assertion fault on a query against a view
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a int);
    create table t2(b int);
    create table t3(c int);
    create view v_test as select b from t2 order by 1;

    set count on;
    select 123 from t1, (select b from v_test union all select c from t3);
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
