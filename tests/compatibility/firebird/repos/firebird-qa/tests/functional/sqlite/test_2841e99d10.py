#coding:utf-8

"""
ID:          2841e99d10
ISSUE:       https://www.sqlite.org/src/tktview/2841e99d10
TITLE:       Different rounding when converting TEXT to DOUBLE PRECISION
DESCRIPTION:
NOTES:
    [17.08.2025] pzotov
    ::: NB ::: Test fails on FB 3.x (issues 2.070934912552031e+18 instead of 2.070934912552030e+18).
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    CREATE TABLE t0(c0 double precision UNIQUE using descending index t0_c0_unq);

    INSERT INTO t0(c0) VALUES(2.07093491255203046E18);
    set count on;
    SELECT * FROM t0 WHERE '2070934912552030444' IN (c0); 
    SELECT * FROM t0 WHERE c0 IN ('2070934912552030444'); 
    SELECT * FROM t0 WHERE c0 IN (2070934912552030444); 
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    C0 2.070934912552030e+18
    Records affected: 1
    C0 2.070934912552030e+18
    Records affected: 1
    C0 2.070934912552030e+18
    Records affected: 1
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
