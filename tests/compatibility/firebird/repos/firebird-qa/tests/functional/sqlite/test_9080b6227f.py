#coding:utf-8

"""
ID:          9080b6227f
ISSUE:       https://www.sqlite.org/src/tktview/9080b6227f
TITLE:       Constant expression in partial index results in row not being fetched
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
    create table t0(c0 int);
    insert into t0(c0) values (0);
    commit;

    create index i0 on t0 computed by(null > c0) where (null is not null);
    set count on;
    select * from t0 where ((null is false) is false);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 0
    Records affected: 1
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
