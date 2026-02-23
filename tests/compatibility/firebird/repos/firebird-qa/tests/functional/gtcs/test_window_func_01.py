#coding:utf-8

"""
ID:          gtcs.window-func-01
TITLE:       Set of miscelaneous tests for verification of windowed functions
DESCRIPTION:
  Statements from this test are added to initial SQL which is stored in: /files/gtcs-window-func.sql
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_WINDOW_FUNC_01.script
FBTEST:      functional.gtcs.window_func_01
"""

import os
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

test_expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Invalid expression in the select list (not contained in either an aggregate function or the GROUP BY clause)

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Cannot use an aggregate or window function in a WHERE clause, use HAVING (for aggregate only) instead

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Cannot use an aggregate or window function in a WHERE clause, use HAVING (for aggregate only) instead
"""

test_expected_stdout = """
    MSG                             point-01
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              1
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              2
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              3
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              4
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              5
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              6
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              7
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              8
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              9
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              10
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              11
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              12
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              13
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              14
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              15
    MSG                             point-02
    COUNT                           16
    COUNT                           15
    MIN                             2.30
    MAX                             15.40
    COUNT                           10
    MIN                             2.30
    MAX                             15.40
    ID                              16
    MSG                             point-04
    COUNT                           4
    COUNT                           3
    MIN                             2.30
    MAX                             3.40
    COUNT                           2
    MIN                             2.30
    MAX                             3.40
    PERSON                          1
    MSG                             point-04
    COUNT                           3
    COUNT                           3
    MIN                             4.30
    MAX                             6.40
    COUNT                           2
    MIN                             4.30
    MAX                             6.40
    PERSON                          2
    MSG                             point-04
    COUNT                           3
    COUNT                           3
    MIN                             6.30
    MAX                             9.40
    COUNT                           2
    MIN                             6.30
    MAX                             9.40
    PERSON                          3
    MSG                             point-04
    COUNT                           3
    COUNT                           3
    MIN                             8.30
    MAX                             12.40
    COUNT                           2
    MIN                             8.30
    MAX                             12.40
    PERSON                          4
    MSG                             point-04
    COUNT                           3
    COUNT                           3
    MIN                             10.30
    MAX                             15.40
    COUNT                           2
    MIN                             10.30
    MAX                             15.40
    PERSON                          5
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             4.30
    MAX                             6.40
    COUNT                           2
    MIN                             4.30
    MAX                             6.40
    PERSON                          2
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             4.30
    MAX                             6.40
    COUNT                           2
    MIN                             4.30
    MAX                             6.40
    PERSON                          2
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             4.30
    MAX                             6.40
    COUNT                           2
    MIN                             4.30
    MAX                             6.40
    PERSON                          2
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             6.30
    MAX                             9.40
    COUNT                           2
    MIN                             6.30
    MAX                             9.40
    PERSON                          3
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             6.30
    MAX                             9.40
    COUNT                           2
    MIN                             6.30
    MAX                             9.40
    PERSON                          3
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             6.30
    MAX                             9.40
    COUNT                           2
    MIN                             6.30
    MAX                             9.40
    PERSON                          3
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             8.30
    MAX                             12.40
    COUNT                           2
    MIN                             8.30
    MAX                             12.40
    PERSON                          4
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             8.30
    MAX                             12.40
    COUNT                           2
    MIN                             8.30
    MAX                             12.40
    PERSON                          4
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             8.30
    MAX                             12.40
    COUNT                           2
    MIN                             8.30
    MAX                             12.40
    PERSON                          4
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             10.30
    MAX                             15.40
    COUNT                           2
    MIN                             10.30
    MAX                             15.40
    PERSON                          5
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             10.30
    MAX                             15.40
    COUNT                           2
    MIN                             10.30
    MAX                             15.40
    PERSON                          5
    MSG                             point-05
    COUNT                           3
    COUNT                           3
    MIN                             10.30
    MAX                             15.40
    COUNT                           2
    MIN                             10.30
    MAX                             15.40
    PERSON                          5
    MSG                             point-05
    COUNT                           4
    COUNT                           3
    MIN                             2.30
    MAX                             3.40
    COUNT                           2
    MIN                             2.30
    MAX                             3.40
    PERSON                          1
    MSG                             point-05
    COUNT                           4
    COUNT                           3
    MIN                             2.30
    MAX                             3.40
    COUNT                           2
    MIN                             2.30
    MAX                             3.40
    PERSON                          1
    MSG                             point-05
    COUNT                           4
    COUNT                           3
    MIN                             2.30
    MAX                             3.40
    COUNT                           2
    MIN                             2.30
    MAX                             3.40
    PERSON                          1
    MSG                             point-05
    COUNT                           4
    COUNT                           3
    MIN                             2.30
    MAX                             3.40
    COUNT                           2
    MIN                             2.30
    MAX                             3.40
    PERSON                          1
    MSG                             point-06
    COUNT                           4
    COUNT                           3
    MIN                             2.30
    MAX                             3.40
    COUNT                           2
    MIN                             2.30
    MAX                             3.40
    NAME                            Person 1
    MSG                             point-06
    COUNT                           3
    COUNT                           3
    MIN                             4.30
    MAX                             6.40
    COUNT                           2
    MIN                             4.30
    MAX                             6.40
    NAME                            Person 2
    MSG                             point-06
    COUNT                           3
    COUNT                           3
    MIN                             6.30
    MAX                             9.40
    COUNT                           2
    MIN                             6.30
    MAX                             9.40
    NAME                            Person 3
    MSG                             point-06
    COUNT                           3
    COUNT                           3
    MIN                             8.30
    MAX                             12.40
    COUNT                           2
    MIN                             8.30
    MAX                             12.40
    NAME                            Person 4
    MSG                             point-06
    COUNT                           3
    COUNT                           3
    MIN                             10.30
    MAX                             15.40
    COUNT                           2
    MIN                             10.30
    MAX                             15.40
    NAME                            Person 5
    MSG                             point-07
    COUNT                           4
    COUNT                           3
    MIN                             2.30
    MAX                             3.40
    COUNT                           2
    MIN                             2.30
    MAX                             3.40
    NAME                            Person 1
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             4.30
    MAX                             6.40
    COUNT                           2
    MIN                             4.30
    MAX                             6.40
    NAME                            Person 2
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             6.30
    MAX                             9.40
    COUNT                           2
    MIN                             6.30
    MAX                             9.40
    NAME                            Person 3
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             8.30
    MAX                             12.40
    COUNT                           2
    MIN                             8.30
    MAX                             12.40
    NAME                            Person 4
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             10.30
    MAX                             15.40
    COUNT                           2
    MIN                             10.30
    MAX                             15.40
    NAME                            Person 5
    MSG                             point-07
    COUNT                           4
    COUNT                           3
    MIN                             2.30
    MAX                             3.40
    COUNT                           2
    MIN                             2.30
    MAX                             3.40
    NAME                            Person 1
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             4.30
    MAX                             6.40
    COUNT                           2
    MIN                             4.30
    MAX                             6.40
    NAME                            Person 2
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             6.30
    MAX                             9.40
    COUNT                           2
    MIN                             6.30
    MAX                             9.40
    NAME                            Person 3
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             8.30
    MAX                             12.40
    COUNT                           2
    MIN                             8.30
    MAX                             12.40
    NAME                            Person 4
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             10.30
    MAX                             15.40
    COUNT                           2
    MIN                             10.30
    MAX                             15.40
    NAME                            Person 5
    MSG                             point-07
    COUNT                           4
    COUNT                           3
    MIN                             2.30
    MAX                             3.40
    COUNT                           2
    MIN                             2.30
    MAX                             3.40
    NAME                            Person 1
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             4.30
    MAX                             6.40
    COUNT                           2
    MIN                             4.30
    MAX                             6.40
    NAME                            Person 2
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             6.30
    MAX                             9.40
    COUNT                           2
    MIN                             6.30
    MAX                             9.40
    NAME                            Person 3
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             8.30
    MAX                             12.40
    COUNT                           2
    MIN                             8.30
    MAX                             12.40
    NAME                            Person 4
    MSG                             point-07
    COUNT                           3
    COUNT                           3
    MIN                             10.30
    MAX                             15.40
    COUNT                           2
    MIN                             10.30
    MAX                             15.40
    NAME                            Person 5
    MSG                             point-07
    COUNT                           4
    COUNT                           3
    MIN                             2.30
    MAX                             3.40
    COUNT                           2
    MIN                             2.30
    MAX                             3.40
    NAME                            Person 1
    MSG                             point-08
    PERSON                          1
    COUNT                           1
    MSG                             point-08
    PERSON                          2
    COUNT                           1
    MSG                             point-08
    PERSON                          3
    COUNT                           1
    MSG                             point-08
    PERSON                          4
    COUNT                           1
    MSG                             point-08
    PERSON                          5
    COUNT                           1
    MSG                             point-09
    PERSON                          1
    COUNT                           1
    MSG                             point-09
    PERSON                          2
    COUNT                           1
    MSG                             point-09
    PERSON                          3
    COUNT                           1
    MSG                             point-09
    PERSON                          4
    COUNT                           1
    MSG                             point-09
    PERSON                          5
    COUNT                           1
    MSG                             point-10
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-10
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-10
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-10
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              4.30
    X4                              6.40
    X5                              2
    X6                              4.30
    X7                              6.40
    X8                              Person 2
    ID                              2
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              4.30
    X4                              6.40
    X5                              2
    X6                              4.30
    X7                              6.40
    X8                              Person 2
    ID                              2
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              4.30
    X4                              6.40
    X5                              2
    X6                              4.30
    X7                              6.40
    X8                              Person 2
    ID                              2
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              6.30
    X4                              9.40
    X5                              2
    X6                              6.30
    X7                              9.40
    X8                              Person 3
    ID                              3
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              6.30
    X4                              9.40
    X5                              2
    X6                              6.30
    X7                              9.40
    X8                              Person 3
    ID                              3
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              6.30
    X4                              9.40
    X5                              2
    X6                              6.30
    X7                              9.40
    X8                              Person 3
    ID                              3
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              8.30
    X4                              12.40
    X5                              2
    X6                              8.30
    X7                              12.40
    X8                              Person 4
    ID                              4
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              8.30
    X4                              12.40
    X5                              2
    X6                              8.30
    X7                              12.40
    X8                              Person 4
    ID                              4
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              8.30
    X4                              12.40
    X5                              2
    X6                              8.30
    X7                              12.40
    X8                              Person 4
    ID                              4
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              10.30
    X4                              15.40
    X5                              2
    X6                              10.30
    X7                              15.40
    X8                              Person 5
    ID                              5
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              10.30
    X4                              15.40
    X5                              2
    X6                              10.30
    X7                              15.40
    X8                              Person 5
    ID                              5
    MSG                             point-10
    X1                              3
    X2                              3
    X3                              10.30
    X4                              15.40
    X5                              2
    X6                              10.30
    X7                              15.40
    X8                              Person 5
    ID                              5
    MSG                             point-11
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-11
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-11
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-11
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              4.30
    X4                              6.40
    X5                              2
    X6                              4.30
    X7                              6.40
    X8                              Person 2
    ID                              2
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              4.30
    X4                              6.40
    X5                              2
    X6                              4.30
    X7                              6.40
    X8                              Person 2
    ID                              2
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              4.30
    X4                              6.40
    X5                              2
    X6                              4.30
    X7                              6.40
    X8                              Person 2
    ID                              2
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              6.30
    X4                              9.40
    X5                              2
    X6                              6.30
    X7                              9.40
    X8                              Person 3
    ID                              3
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              6.30
    X4                              9.40
    X5                              2
    X6                              6.30
    X7                              9.40
    X8                              Person 3
    ID                              3
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              6.30
    X4                              9.40
    X5                              2
    X6                              6.30
    X7                              9.40
    X8                              Person 3
    ID                              3
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              8.30
    X4                              12.40
    X5                              2
    X6                              8.30
    X7                              12.40
    X8                              Person 4
    ID                              4
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              8.30
    X4                              12.40
    X5                              2
    X6                              8.30
    X7                              12.40
    X8                              Person 4
    ID                              4
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              8.30
    X4                              12.40
    X5                              2
    X6                              8.30
    X7                              12.40
    X8                              Person 4
    ID                              4
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              10.30
    X4                              15.40
    X5                              2
    X6                              10.30
    X7                              15.40
    X8                              Person 5
    ID                              5
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              10.30
    X4                              15.40
    X5                              2
    X6                              10.30
    X7                              15.40
    X8                              Person 5
    ID                              5
    MSG                             point-11
    X1                              3
    X2                              3
    X3                              10.30
    X4                              15.40
    X5                              2
    X6                              10.30
    X7                              15.40
    X8                              Person 5
    ID                              5
    MSG                             point-12
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-12
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-12
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-12
    X1                              4
    X2                              3
    X3                              2.30
    X4                              3.40
    X5                              2
    X6                              2.30
    X7                              3.40
    X8                              Person 1
    ID                              1
    MSG                             point-12
    X1                              3
    X2                              3
    X3                              6.30
    X4                              9.40
    X5                              2
    X6                              6.30
    X7                              9.40
    X8                              Person 3
    ID                              3
    MSG                             point-12
    X1                              3
    X2                              3
    X3                              6.30
    X4                              9.40
    X5                              2
    X6                              6.30
    X7                              9.40
    X8                              Person 3
    ID                              3
    MSG                             point-12
    X1                              3
    X2                              3
    X3                              6.30
    X4                              9.40
    X5                              2
    X6                              6.30
    X7                              9.40
    X8                              Person 3
    ID                              3
    MSG                             point-13
    X3                              2.30
    SUM                             13.60
    MSG                             point-13
    X3                              4.30
    SUM                             19.20
    MSG                             point-13
    X3                              6.30
    SUM                             28.20
    MSG                             point-13
    X3                              8.30
    SUM                             37.20
    MSG                             point-13
    X3                              10.30
    SUM                             46.20
    MSG                             point-14
    X3                              2.30
    SUM                             13.60
    COUNT                           5
    MSG                             point-14
    X3                              4.30
    SUM                             19.20
    COUNT                           5
    MSG                             point-14
    X3                              6.30
    SUM                             28.20
    COUNT                           5
    MSG                             point-14
    X3                              8.30
    SUM                             37.20
    COUNT                           5
    MSG                             point-14
    X3                              10.30
    SUM                             46.20
    COUNT                           5
    MSG                             point-15
    X3                              2.30
    SUM                             13.60
    SUM                             144.40
    MSG                             point-15
    X3                              4.30
    SUM                             19.20
    SUM                             144.40
    MSG                             point-15
    X3                              6.30
    SUM                             28.20
    SUM                             144.40
    MSG                             point-15
    X3                              8.30
    SUM                             37.20
    SUM                             144.40
    MSG                             point-15
    X3                              10.30
    SUM                             46.20
    SUM                             144.40
    MSG                             point-17
    PERSON                          1
    SUM                             9.10
    COUNT                           5
    MSG                             point-17
    PERSON                          2
    SUM                             17.10
    COUNT                           5
    MSG                             point-17
    PERSON                          3
    SUM                             25.10
    COUNT                           5
    MSG                             point-17
    PERSON                          4
    SUM                             33.10
    COUNT                           5
    MSG                             point-17
    PERSON                          5
    SUM                             41.10
    COUNT                           5
    MSG                             point-18
    PERSON                          1
    NAME                            Person 1
    SUM                             9.10
    COUNT                           5
    SUM                             125.50
    MSG                             point-18
    PERSON                          2
    NAME                            Person 2
    SUM                             17.10
    COUNT                           5
    SUM                             125.50
    MSG                             point-18
    PERSON                          3
    NAME                            Person 3
    SUM                             25.10
    COUNT                           5
    SUM                             125.50
    MSG                             point-18
    PERSON                          4
    NAME                            Person 4
    SUM                             33.10
    COUNT                           5
    SUM                             125.50
    MSG                             point-18
    PERSON                          5
    NAME                            Person 5
    SUM                             41.10
    COUNT                           5
    SUM                             125.50
    MSG                             point-19
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-19
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-19
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-19
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-19
    PERSON                          2
    COUNT                           16
    COUNT                           3
    MSG                             point-19
    PERSON                          2
    COUNT                           16
    COUNT                           3
    MSG                             point-19
    PERSON                          2
    COUNT                           16
    COUNT                           3
    MSG                             point-19
    PERSON                          3
    COUNT                           16
    COUNT                           3
    MSG                             point-19
    PERSON                          3
    COUNT                           16
    COUNT                           3
    MSG                             point-19
    PERSON                          3
    COUNT                           16
    COUNT                           3
    MSG                             point-19
    PERSON                          4
    COUNT                           16
    COUNT                           3
    MSG                             point-19
    PERSON                          4
    COUNT                           16
    COUNT                           3
    MSG                             point-19
    PERSON                          4
    COUNT                           16
    COUNT                           3
    MSG                             point-19
    PERSON                          5
    COUNT                           16
    COUNT                           3
    MSG                             point-19
    PERSON                          5
    COUNT                           16
    COUNT                           3
    MSG                             point-19
    PERSON                          5
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-20
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-20
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-20
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-20
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-20
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-20
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-20
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-20
    PERSON                          2
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          2
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          2
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          2
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          2
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          2
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          3
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          3
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          3
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          3
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          3
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          3
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          4
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          4
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          4
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          4
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          4
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          4
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          5
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          5
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          5
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          5
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          5
    COUNT                           16
    COUNT                           3
    MSG                             point-20
    PERSON                          5
    COUNT                           16
    COUNT                           3
    MSG                             point-21
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    COUNT                           4
    MSG                             point-21
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    COUNT                           3
    MSG                             point-21
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    COUNT                           3
    MSG                             point-21
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    COUNT                           3
    MSG                             point-21
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    COUNT                           3
    MSG                             point-21
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    COUNT                           4
    MSG                             point-21
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    COUNT                           3
    MSG                             point-21
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    COUNT                           3
    MSG                             point-21
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    COUNT                           3
    MSG                             point-21
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    COUNT                           3
    MSG                             point-21
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    COUNT                           4
    MSG                             point-21
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    COUNT                           3
    MSG                             point-21
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    COUNT                           3
    MSG                             point-21
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    COUNT                           3
    MSG                             point-21
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    COUNT                           3
    MSG                             point-21
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    COUNT                           4
    MSG                             point-22
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    COUNT                           16
    COUNT                           15
    COUNT                           4
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    COUNT                           16
    COUNT                           15
    COUNT                           4
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    COUNT                           16
    COUNT                           15
    COUNT                           4
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           1
    COUNT                           1
    MSG                             point-22
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    COUNT                           16
    COUNT                           15
    COUNT                           4
    COUNT                           3
    COUNT                           1
    COUNT                           0
    MSG                             point-23
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    COUNT                           16
    COUNT                           15
    COUNT                           4
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    COUNT                           16
    COUNT                           15
    COUNT                           4
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    COUNT                           16
    COUNT                           15
    COUNT                           4
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    COUNT                           16
    COUNT                           15
    COUNT                           3
    COUNT                           3
    COUNT                           5
    COUNT                           5
    MSG                             point-23
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    COUNT                           16
    COUNT                           15
    COUNT                           4
    COUNT                           3
    COUNT                           1
    COUNT                           0
    MSG                             point-24
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    MIN                             2010-01-03
    MAX                             2010-03-02
    MSG                             point-24
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    MIN                             2010-01-04
    MAX                             2010-03-03
    MSG                             point-24
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    MIN                             2010-01-05
    MAX                             2010-03-04
    MSG                             point-24
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    MIN                             2010-01-06
    MAX                             2010-03-05
    MSG                             point-24
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    MIN                             2010-01-07
    MAX                             2010-03-06
    MSG                             point-24
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    MIN                             2010-01-03
    MAX                             2010-03-02
    MSG                             point-24
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    MIN                             2010-01-04
    MAX                             2010-03-03
    MSG                             point-24
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    MIN                             2010-01-05
    MAX                             2010-03-04
    MSG                             point-24
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    MIN                             2010-01-06
    MAX                             2010-03-05
    MSG                             point-24
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    MIN                             2010-01-07
    MAX                             2010-03-06
    MSG                             point-24
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    MIN                             2010-01-03
    MAX                             2010-03-02
    MSG                             point-24
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    MIN                             2010-01-04
    MAX                             2010-03-03
    MSG                             point-24
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    MIN                             2010-01-05
    MAX                             2010-03-04
    MSG                             point-24
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    MIN                             2010-01-06
    MAX                             2010-03-05
    MSG                             point-24
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    MIN                             2010-01-07
    MAX                             2010-03-06
    MSG                             point-24
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    MIN                             2010-01-03
    MAX                             2010-03-02
    MSG                             point-25
    PERSON                          1
    MIN                             2010-01-03
    MAX                             2010-03-02
    MSG                             point-25
    PERSON                          2
    MIN                             2010-01-04
    MAX                             2010-03-03
    MSG                             point-25
    PERSON                          3
    MIN                             2010-01-05
    MAX                             2010-03-04
    MSG                             point-25
    PERSON                          4
    MIN                             2010-01-06
    MAX                             2010-03-05
    MSG                             point-25
    PERSON                          5
    MIN                             2010-01-07
    MAX                             2010-03-06
    MSG                             point-26
    PERSON                          1
    COUNT                           16
    COUNT                           4
    MSG                             point-26
    PERSON                          2
    COUNT                           16
    COUNT                           3
    MSG                             point-26
    PERSON                          3
    COUNT                           16
    COUNT                           3
    MSG                             point-26
    PERSON                          4
    COUNT                           16
    COUNT                           3
    MSG                             point-26
    PERSON                          5
    COUNT                           16
    COUNT                           3
    MSG                             point-27
    PERSON                          1
    COUNT                           4
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-27
    PERSON                          2
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-27
    PERSON                          3
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-27
    PERSON                          4
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-27
    PERSON                          5
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-28
    PERSON                          5
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-28
    PERSON                          4
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-28
    PERSON                          3
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-28
    PERSON                          2
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-28
    PERSON                          1
    COUNT                           4
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-29
    PERSON                          5
    C1                              3
    C2                              5
    C3                              1
    C4                              5
    MSG                             point-29
    PERSON                          4
    C1                              3
    C2                              5
    C3                              1
    C4                              5
    MSG                             point-29
    PERSON                          3
    C1                              3
    C2                              5
    C3                              1
    C4                              5
    MSG                             point-29
    PERSON                          2
    C1                              3
    C2                              5
    C3                              1
    C4                              5
    MSG                             point-29
    PERSON                          1
    C1                              4
    C2                              5
    C3                              1
    C4                              5
    MSG                             point-30
    PERSON                          5
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-30
    PERSON                          4
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-30
    PERSON                          3
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-30
    PERSON                          2
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-30
    PERSON                          1
    COUNT                           4
    COUNT                           5
    COUNT                           1
    COUNT                           5
    MSG                             point-31
    PERSON                          5
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    COUNT                           5
    MSG                             point-31
    PERSON                          4
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    COUNT                           5
    MSG                             point-31
    PERSON                          3
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    COUNT                           5
    MSG                             point-31
    PERSON                          2
    COUNT                           3
    COUNT                           5
    COUNT                           1
    COUNT                           5
    COUNT                           5
    MSG                             point-31
    PERSON                          1
    COUNT                           4
    COUNT                           5
    COUNT                           1
    COUNT                           5
    COUNT                           5
    MSG                             point-32
    PERSON                          5
    COUNT                           3
    COUNT                           4
    COUNT                           1
    COUNT                           4
    COUNT                           4
    MSG                             point-32
    PERSON                          4
    COUNT                           3
    COUNT                           4
    COUNT                           1
    COUNT                           4
    COUNT                           4
    MSG                             point-32
    PERSON                          3
    COUNT                           3
    COUNT                           4
    COUNT                           1
    COUNT                           4
    COUNT                           4
    MSG                             point-32
    PERSON                          2
    COUNT                           3
    COUNT                           4
    COUNT                           1
    COUNT                           4
    COUNT                           4
    MSG                             point-33
    PERSON                          5
    SUM                             41.10
    COUNT                           5
    COUNT                           1
    COUNT                           5
    COUNT                           5
    MSG                             point-33
    PERSON                          4
    SUM                             33.10
    COUNT                           5
    COUNT                           1
    COUNT                           5
    COUNT                           5
    MSG                             point-33
    PERSON                          3
    SUM                             25.10
    COUNT                           5
    COUNT                           1
    COUNT                           5
    COUNT                           5
    MSG                             point-33
    PERSON                          2
    SUM                             17.10
    COUNT                           5
    COUNT                           1
    COUNT                           5
    COUNT                           5
    MSG                             point-33
    PERSON                          1
    SUM                             9.10
    COUNT                           5
    COUNT                           1
    COUNT                           5
    COUNT                           5
    MSG                             point-34
    PERSON                          3
    SUM                             25.10
    COUNT                           2
    COUNT                           1
    COUNT                           2
    COUNT                           2
    MSG                             point-34
    PERSON                          2
    SUM                             17.10
    COUNT                           2
    COUNT                           1
    COUNT                           2
    COUNT                           2
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    sql_init = (act.files_dir / 'gtcs-window-func.sql').read_text()

    sql_addi= \
    """
        set list on;
        select
            'point-01' as msg,
            count(*), count(val), min(val), max(val),
            count(distinct val), min(distinct val), max(distinct val)
          from entries;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


        select
            'point-02' as msg,
            count(*) over (), count(val) over (), min(val) over (), max(val) over (),
            count(distinct val) over (), min(distinct val) over (), max(distinct val) over (),
            id
          from entries
          order by id;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-03' as msg,
            count(*) over (), count(val) over (), min(val) over (), max(val) over (),
            count(distinct val) over (), min(distinct val) over (), max(distinct val) over (),
            id
          from entries
          where 1 = 0
          order by id;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-04' as msg,
            count(*), count(val), min(val), max(val),
            count(distinct val), min(distinct val), max(distinct val),
            person
          from entries
          group by person
          order by person;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-05' as msg,
            count(*) over (partition by person),
            count(val) over (partition by person),
            min(val) over (partition by person),
            max(val) over (partition by person),
            count(distinct val) over (partition by person),
            min(distinct val) over (partition by person),
            max(distinct val) over (partition by person),
            person
          from entries
          order by 1, 2, 3, 4, 5, 6, 7, 8, 9;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-06' as msg,
            count(*),
            count(e.val),
            min(e.val),
            max(e.val),
            count(distinct e.val),
            min(distinct e.val),
            max(distinct e.val),
            p.name
          from entries e
          join persons p on p.id = e.person
          group by p.name
          order by p.name;

         --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-07' as msg,
            count(*) over (partition by p.id),
            count(e.val) over (partition by p.id),
            min(e.val) over (partition by p.id),
            max(e.val) over (partition by p.id),
            count(distinct e.val) over (partition by p.id),
            min(distinct e.val) over (partition by p.id),
            max(distinct e.val) over (partition by p.id),
            p.name
          from entries e
          join persons p on p.id = e.person
          order by e.id;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-08' as msg,
            person,
            count(person) over (partition by person)
          from entries
          group by person
          order by person;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-09' as msg,
            person,
            count(*) over (partition by person)
          from entries
          group by person
          order by person;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
          'point-10' as msg,
          v1.*, p.id
          from persons p
          join v1 on v1.x8 = p.name;

        select
          'point-11' as msg,
          v1.*, p.id
          from persons p
          full join v1 on right(v1.x8, 1) = p.id;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
          'point-12' as msg,
          v1.*, p.id
          from persons p
          left join v1 on right(v1.x8, 1) = p.id
          where p.id in (1, 3);

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
          'point-13' as msg,
          x3, sum(x4)
          from v1
          group by x3;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
          'point-14' as msg,
          x3, sum(x4), count(*)over()
          from v1
          group by x3;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
          'point-15' as msg,
          x3, sum(x4), sum(sum(x4))over()
          from v1
          group by x3;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
          'point-17' as msg,
          v2.person, sum(v2.val), count (*) over ()
          from v2
          join persons p
            on p.id = v2.person
          group by person;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
          'point-18' as msg,
          v3.person, v3.name, sum(v3.val), count (*) over (), sum(sum(v3.val)) over ()
          from v3
          join persons p
            on p.id = v3.person
          group by person, name;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-19' as msg,
            person,
            count(*) over (),
            count(*) over (partition by person)
          from entries
          order by 1, 2, 3, 4;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-20' as msg,
            person,
            count(*) over (),
            count(*) over (partition by person)
          from entries

        UNION ALL

        select
            'point-20' as msg,
            person,
            count(*) over (),
            count(*) over (partition by person)
          from entries
          order by 1, 2, 3, 4;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-21' as msg,
            entries.*,
            count(*) over (partition by person || person)
          from entries
          order by 2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-22' as msg,
            entries.*,
            count(*) over (),
            count(val) over (),
            count(*) over (partition by person),
            count(val) over (partition by person),
            count(*) over (partition by dat),
            count(val) over (partition by dat)
          from entries
          order by 2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-23' as msg,
            entries.*,
            count(*) over (),
            count(val) over (),
            count(*) over (partition by person),
            count(val) over (partition by person),
            count(*) over (partition by extract(month from dat)),
            count(val) over (partition by extract(month from dat))
          from entries
          order by 2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-24' as msg,
            entries.*,
            min(dat) over (partition by person),
            max(dat) over (partition by person)
          from entries
          order by 2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select distinct
            'point-25' as msg,
            person,
            min(dat) over (partition by person),
            max(dat) over (partition by person)
          from entries;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select distinct
            'point-26' as msg,
            person,
            count(*) over (),
            count(*) over (partition by person)
          from entries
          order by 2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-27' as msg,
            person,
            count(*),
            count(*) over (),
            count(*) over (partition by person),
            count(*) over (partition by 1, 2, 3)
          from entries
          group by person
          order by 2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-28' as msg,
            person,
            count(*),
            count(*) over (),
            count(*) over (partition by person),
            count(*) over (partition by 1, 2, 3)
          from entries
          group by person
          order by 2 desc;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select *
          from (
            select
                'point-29' as msg,
                person,
                count(*) c1,
                count(*) over () c2,
                count(*) over (partition by person) c3,
                count(*) over (partition by 1, 2, 3) c4
              from entries
              group by person
          )
          order by 2 desc;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-30' as msg,
            person,
            count(*),
            count(*) over (),
            count(*) over (partition by person),
            count(*) over (partition by 1, 2, 3)
          from entries
          group by person
          order by 2 desc;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-31' as msg,
            person,
            count(*),
            count(*) over (),
            count(*) over (partition by person),
            count(*) over (partition by 1, 2, 3),
            count(count(*)) over ()
          from entries
          group by person
          order by 4 desc, 2 desc;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-32' as msg,
            person,
            count(*),
            count(*) over (),
            count(*) over (partition by person),
            count(*) over (partition by 1, 2, 3),
            count(count(*)) over ()
          from entries
          group by person
          having count(*) = 3
          order by 4 desc, 2 desc;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-33' as msg,
            person,
            sum(val),
            count(*) over (),
            count(*) over (partition by person),
            count(*) over (partition by 1, 2, 3),
            count(count(*)) over ()
          from entries
          group by person
          order by 4 desc, 2 desc;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-34' as msg,
            person,
            sum(val),
            count(*) over (),
            count(*) over (partition by person),
            count(*) over (partition by 1, 2, 3),
            count(count(*)) over ()
          from entries
          group by person
          having sum(val) between 16 and 26
          order by 4 desc, 2 desc;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        -- Test invalid usages. Following statements must raise error:
        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select
            'point-35' as msg,
            person,
            sum(val) over (partition by count(*))
          from entries;

        select
            'point-36' as msg,
            person
          from entries
          where count(*) over () = 1;

        select
            'point-37' as msg,
            person
          from entries
          group by person
          having count(*) over () = 1;
     """

    act.expected_stdout = test_expected_stdout
    act.expected_stderr = test_expected_stderr

    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ) )

    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)
