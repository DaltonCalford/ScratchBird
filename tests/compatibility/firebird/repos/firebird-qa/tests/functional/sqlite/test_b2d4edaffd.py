#coding:utf-8

"""
ID:          b2d4edaffd
ISSUE:       https://www.sqlite.org/src/tktview/b2d4edaffd
TITLE:       Comparison on view malfunctions
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c0 int);
    create view v0(c0) as select row_number()over() from t0 order by 1;
    insert into t0(c0) values (0);

    set count on;
    select count(*) from v0 where abs('1') = v0.c0; -- expected: 1, actual: 0
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    COUNT 1
    Records affected: 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
