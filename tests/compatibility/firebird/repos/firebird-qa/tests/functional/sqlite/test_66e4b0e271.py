#coding:utf-8

"""
ID:          66e4b0e271
ISSUE:       https://www.sqlite.org/src/tktview/66e4b0e271
TITLE:       Incorrect result for LEFT JOIN
DESCRIPTION:
NOTES:
    [14.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c0 boolean);
    create view v0(c0) as select null and null from t0;
    insert into t0(c0) values (null);
    set count on;
    --select v0.c0 as v_c0, t0.c0 as t_c0 from v0 natural join t0; -- expected: {null|null}, actual: {}
    --select v0.c0 as v_c0, t0.c0 as t_c0 from v0 join t0 on v0.c0 is not distinct from t0.c0; -- expected: {null|null}, actual: {}
    select v0.c0 as v_c0, t0.c0 as t_c0 from v0 left join t0 on v0.c0; -- expected: {null|null}, actual: {}
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    V_C0 <null>
    T_C0 <null>
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
