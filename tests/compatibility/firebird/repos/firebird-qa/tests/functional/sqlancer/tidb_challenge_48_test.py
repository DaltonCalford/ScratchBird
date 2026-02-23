#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/tidb-challenge-program/bug-hunting-issue/issues/19
TITLE:       UNIQUE constraint on DECIMAL/floating-point columns causes incorrect result for NULL in AND
DESCRIPTION:
    https://github.com/sqlancer/sqlancer/blob/main/CONTRIBUTING.md#unfixed-bugs
    https://github.com/sqlancer/sqlancer/blob/4c20a94b3ad2c037e1a66c0b637184f8c20faa7e/src/sqlancer/tidb/TiDBBugs.java
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    CREATE TABLE t0(c0 DOUBLE PRECISION UNIQUE);
    INSERT INTO t0(c0) VALUES (NULL);
    SELECT t0.c0 FROM t0 WHERE NOT (t0.c0 is null AND true); -- expected: {}, actual: {NULL}
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute(combine_output = True)
    assert act.clean_stdout == ''
