#coding:utf-8

"""
ID:          n/a
ISSUE:       https://docs.google.com/document/d/1N-oUGVATV0l6tG87uOtPNmfLS7g_fuo7HIckFobD-Yo/edit?tab=t.0
TITLE:       Bugs Found in StoneDB by SQLancer
DESCRIPTION:
    This test contains several tiny examples provided in "GSoC 2023: Midterm Report on Support of StoneDB"
NOTES:
    See also:
    https://sqlancer.github.io/blog/gsoc-sqlancer-midterm-zhenglin/
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- https://github.com/stoneatom/stonedb/issues/1941
    -- StoneDB will crash when executing the command:
    set heading off;
    select 'stonedb_1941' as msg from rdb$database;
    recreate table t0 (c0 int);
    SELECT *
    FROM t0
    GROUP BY t0.c0
    HAVING (t0.c0 IS NULL)
    UNION ALL
    SELECT *
    FROM t0
    GROUP BY t0.c0
    HAVING (NOT (t0.c0 IS NULL))
    UNION ALL
    SELECT *
    FROM t0
    GROUP BY t0.c0
    HAVING (((t0.c0 IS NULL)) IS NULL);
    commit;
    --###################################################
    -- https://github.com/stoneatom/stonedb/issues/1952
    -- StoneDB crash: PRIMARY KEY, HAVING, IS NULL
    select 'stonedb_1952' as msg from rdb$database;
    RECREATE TABLE t0(c0 INT PRIMARY KEY);
    -- NB: mysql *allows* such query and issues empty resultset.
    -- FB raises "Invalid expression in the select list":
    SELECT * FROM t0 HAVING (t0.c0 IS NULL);
    commit;
    --###################################################
    -- https://github.com/stoneatom/stonedb/issues/1954
    -- StoneDB crash: HAVING, IS NULL
    select 'stonedb_1954' as msg from rdb$database;
    RECREATE TABLE t0(c0 INT PRIMARY KEY);
    -- NB: mysql *allows* such query and issues empty resultset.
    -- FB raises "Invalid expression in the select list":
    SELECT c0 FROM t0 HAVING (1 IS NULL);
    commit;

    --###################################################
    -- https://github.com/stoneatom/stonedb/issues/1949
    -- StoneDB crash: HAVING, IS NULL
    select 'stonedb_1949' as msg from rdb$database;
    RECREATE TABLE t0(c0 blob);
    INSERT INTO t0 DEFAULT values;
    DELETE FROM t0;
    INSERT INTO t0 DEFAULT values;
    SELECT * FROM t0;
    commit;
    --###################################################
    -- https://github.com/stoneatom/stonedb/issues/1950
    -- query result wrong: CASE WHEN THEN ELSE
    select 'stonedb_1950' as msg from rdb$database;
    RECREATE TABLE t0(c0 INT);
    INSERT INTO t0 default values;
    SELECT * FROM t0 WHERE (CASE (t0.c0 IN (t0.c0)) WHEN TRUE THEN 'TRUE' ELSE 'FALSE' END) = true;
    commit;
    --###################################################
    -- https://github.com/stoneatom/stonedb/issues/1955
    -- query result wrong: ALTER, DEFAULT
    select 'stonedb_1955' as msg from rdb$database;
    RECREATE TABLE t0(c0 INT) ;
    INSERT INTO t0 DEFAULT VALUES;
    COMMIT;
    ALTER TABLE t0 ADD c1 INT DEFAULT 1;
    INSERT INTO t0 DEFAULT VALUES;
    SELECT * FROM t0 order by c0,c1 nulls first;
    commit;

    --###################################################
    -- https://docs.google.com/document/d/1N-oUGVATV0l6tG87uOtPNmfLS7g_fuo7HIckFobD-Yo/edit?tab=t.0
    -- BUGS OR CRASHES FOUND BUT NOT REPORTED
    -- query result wrong: >=, IS NULL
    -- expected empty set:
    select 'stonedb_non_reported_01' as msg from rdb$database;
    RECREATE TABLE t0(c0 INT) ;
    INSERT INTO t0 default VALUES;
    SELECT t0.c0 FROM t0 WHERE (('false')>=(((t0.c0) IS NULL)));
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = """
        stonedb_1941
        
        stonedb_1952
        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -104
        -Invalid expression in the select list (not contained in either an aggregate function or the GROUP BY clause)

        stonedb_1954
        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -104
        -Invalid expression in the select list (not contained in either an aggregate function or the GROUP BY clause)

        stonedb_1949
        <null>

        stonedb_1950

        stonedb_1955
        <null> <null>
        <null> 1

        stonedb_non_reported_01
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
