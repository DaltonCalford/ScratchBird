#coding:utf-8

"""
ID:          n/a
ISSUE:       https://arxiv.org/pdf/2312.17510
TITLE:       PARTIAL INDICES. NATURAL RIGHT JOIN results in an unexpected "Unknown column" error
DESCRIPTION:
    https://arxiv.org/pdf/2312.17510 page #2 listing 1
NOTES:
    [05.06.2025] pzotov
    Support for partial indices in FB:
    https://github.com/FirebirdSQL/firebird/pull/7257
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    --Listing 1. A bug found by QPG in SQLite due to an incorrect use of an
    --index in combination with a JOIN. Given the same SELECT, the left query
    --plan is produced if no index is present, while the right one uses the index.
    CREATE TABLE t1(a INT, b INT);
    CREATE TABLE t2(c INT);
    CREATE TABLE t3(d INT);

    INSERT INTO t1(a) VALUES(2);
    INSERT INTO t3 VALUES(1);
    commit;

    CREATE INDEX i0 ON t2(c)
    WHERE c = 3 -- ::: NB ::: partial index
    ;

    SELECT *
    FROM t2
    RIGHT JOIN t3 ON d <> 0
    LEFT JOIN t1 ON c = 3
    WHERE t1.a <> 0; -- output must be empty resultset
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.execute(combine_output = True)
    assert act.clean_stdout == ''
