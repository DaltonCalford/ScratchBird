#coding:utf-8

"""
ID:          16028
ISSUE:       https://github.com/pingcap/tidb/issues/16028
TITLE:       Incorrect result when comparing a FLOAT/DOUBLE UNSIGNED with a negative number
DESCRIPTION:
    https://github.com/sqlancer/sqlancer/blob/main/CONTRIBUTING.md#unfixed-bugs
    https://github.com/sqlancer/sqlancer/blob/4c20a94b3ad2c037e1a66c0b637184f8c20faa7e/src/sqlancer/tidb/TiDBBugs.java
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    CREATE TABLE t0(c0 double precision unique);
    INSERT INTO t0(c0) VALUES (0);
    SELECT * FROM t0 WHERE t0.c0 = -1; -- expected: {}, actual: {0}
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute(combine_output = True)
    assert act.clean_stdout == ''
