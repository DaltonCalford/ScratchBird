#coding:utf-8

"""
ID:          15844
ISSUE:       https://github.com/pingcap/tidb/issues/15844
TITLE:       NATURAL RIGHT JOIN results in an unexpected "Unknown column" error
DESCRIPTION:
    https://github.com/sqlancer/sqlancer/blob/main/CONTRIBUTING.md#unfixed-bugs
    https://github.com/sqlancer/sqlancer/blob/4c20a94b3ad2c037e1a66c0b637184f8c20faa7e/src/sqlancer/tidb/TiDBBugs.java
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    CREATE TABLE t0(c0 boolean);
    CREATE TABLE t1(c0 boolean);
    SELECT t0.c0 FROM t0 NATURAL RIGHT JOIN t1 WHERE t1.c0; -- Unknown column 't0.c0' in 'field list'
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute(combine_output = True)
    assert act.clean_stdout == ''
