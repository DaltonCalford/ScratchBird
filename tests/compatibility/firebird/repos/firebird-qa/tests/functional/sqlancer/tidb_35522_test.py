#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/pingcap/tidb/issues/35522
TITLE:       incorrect unresolved column when using natural join
DESCRIPTION:
    https://github.com/sqlancer/sqlancer/blob/main/CONTRIBUTING.md#unfixed-bugs
    https://github.com/sqlancer/sqlancer/blob/4c20a94b3ad2c037e1a66c0b637184f8c20faa7e/src/sqlancer/tidb/TiDBBugs.java
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    CREATE TABLE t0(c0 CHAR);
    CREATE TABLE t1(c0 CHAR);
    SELECT t1.c0 FROM t1 NATURAL RIGHT JOIN t0 WHERE true IS NULL; -- ERROR 1054 (42S22) at line 4: Unknown column 't1.c0' in 'field list'
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute(combine_output = True)
    assert act.clean_stdout == ''
