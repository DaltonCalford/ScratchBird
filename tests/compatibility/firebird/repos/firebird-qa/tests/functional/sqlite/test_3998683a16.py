#coding:utf-8

"""
ID:          3998683a16
ISSUE:       https://www.sqlite.org/src/tktview/3998683a16
TITLE:       Some valid floating-point literals are not recognized.
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
    create table t1(id int, y char(10));
    insert into t1 values( 1, '1.0') returning cast(y as double precision);
    insert into t1 values( 2, '.125') returning cast(y as double precision);
    insert into t1 values( 3, '123.') returning cast(y as double precision);
    insert into t1 values( 4, '123.e+2') returning cast(y as double precision);
    insert into t1 values( 5, '.125e+3') returning cast(y as double precision);
    insert into t1 values( 6, '123e4') returning cast(y as double precision);

    insert into t1 values( 7, '-1.0') returning cast(y as double precision);
    insert into t1 values( 8, '-.125') returning cast(y as double precision);
    insert into t1 values( 9, '-123.') returning cast(y as double precision);
    insert into t1 values(10, '-123.e+2') returning cast(y as double precision);
    insert into t1 values(11, '-.125e+3') returning cast(y as double precision);
    insert into t1 values(12, '-123e4') returning cast(y as double precision);
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    CAST 1.000000000000000
    CAST 0.1250000000000000
    CAST 123.0000000000000
    CAST 12300.00000000000
    CAST 125.0000000000000
    CAST 1230000.000000000
    CAST -1.000000000000000
    CAST -0.1250000000000000
    CAST -123.0000000000000
    CAST -12300.00000000000
    CAST -125.0000000000000
    CAST -1230000.000000000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
