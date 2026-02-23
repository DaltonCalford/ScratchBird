#coding:utf-8

"""
ID:          n/a
ISSUE:       https://arxiv.org/pdf/2312.17510
TITLE:       Bug in RIGHT JOIN
DESCRIPTION:
    https://arxiv.org/pdf/2312.17510 page #7 listing 2
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    CREATE TABLE t1(a CHAR);
    CREATE TABLE t2(b CHAR);
    CREATE TABLE t3(c CHAR NOT NULL);
    CREATE TABLE t4(d CHAR);

    INSERT INTO t2 VALUES('x');
    INSERT INTO t3 VALUES('y');

    SELECT *
    FROM t4
    LEFT JOIN t3 ON TRUE
    INNER JOIN t1 ON t3.c=''
    RIGHT JOIN t2 ON t3.c=''
    WHERE t3.c IS NULL; 

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = """
        D <null>
        C <null>
        A <null>
        B x
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
