#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/tidb-challenge-program/bug-hunting-issue/issues/19
TITLE:       Incorrect result for LEFT JOIN and CASE operator
DESCRIPTION:
    https://github.com/sqlancer/sqlancer/blob/main/CONTRIBUTING.md#unfixed-bugs
    https://github.com/sqlancer/sqlancer/blob/4c20a94b3ad2c037e1a66c0b637184f8c20faa7e/src/sqlancer/tidb/TiDBBugs.java
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    CREATE TABLE t0(c0 INT);
    CREATE TABLE t1(c0 INT);
    INSERT INTO t0 VALUES (0);
    INSERT INTO t1 VALUES (0);
    SELECT * FROM t1 LEFT JOIN t0 ON t0.c0 = t1.c0 WHERE (CASE t0.c0 WHEN 0 THEN t1.c0 ELSE 1 END) <> 0; -- expected: {}, actual: {0|NULL}
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute(combine_output = True)
    assert act.clean_stdout == ''
