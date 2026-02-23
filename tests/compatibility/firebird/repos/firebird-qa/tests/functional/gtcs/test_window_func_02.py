#coding:utf-8

"""
ID:          gtcs.window-func-02
TITLE:       Set of miscelaneous tests for verification of windowed functions
DESCRIPTION:
  Statements from this test are added to initial SQL which is stored in: /files/gtcs-window-func.sql
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_WINDOW_FUNC_02.script
FBTEST:      functional.gtcs.window_func_02
"""

import os
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

test_expected_stdout = """
    MSG                             point-01
    ID                              1
    NAME                            Person 1
    SUM                             1
    MSG                             point-01
    ID                              2
    NAME                            Person 2
    SUM                             2
    MSG                             point-01
    ID                              3
    NAME                            Person 3
    SUM                             3
    MSG                             point-01
    ID                              4
    NAME                            Person 4
    SUM                             4
    MSG                             point-01
    ID                              5
    NAME                            Person 5
    SUM                             5
    MSG                             point-02
    ID                              1
    NAME                            Person 1
    SUM                             5
    MSG                             point-02
    ID                              2
    NAME                            Person 2
    SUM                             4
    MSG                             point-02
    ID                              3
    NAME                            Person 3
    SUM                             3
    MSG                             point-02
    ID                              4
    NAME                            Person 4
    SUM                             2
    MSG                             point-02
    ID                              5
    NAME                            Person 5
    SUM                             1
    MSG                             point-03
    ID                              1
    NAME                            Person 1
    SUM                             1
    MSG                             point-03
    ID                              2
    NAME                            Person 2
    SUM                             2
    MSG                             point-03
    ID                              3
    NAME                            Person 3
    SUM                             3
    MSG                             point-03
    ID                              4
    NAME                            Person 4
    SUM                             4
    MSG                             point-03
    ID                              5
    NAME                            Person 5
    SUM                             5
    MSG                             point-04
    ID                              5
    NAME                            Person 5
    SUM                             5
    MSG                             point-04
    ID                              4
    NAME                            Person 4
    SUM                             4
    MSG                             point-04
    ID                              3
    NAME                            Person 3
    SUM                             3
    MSG                             point-04
    ID                              2
    NAME                            Person 2
    SUM                             2
    MSG                             point-04
    ID                              1
    NAME                            Person 1
    SUM                             1
    MSG                             point-05
    ID                              5
    NAME                            Person 5
    SUM                             1
    MSG                             point-05
    ID                              4
    NAME                            Person 4
    SUM                             2
    MSG                             point-05
    ID                              3
    NAME                            Person 3
    SUM                             3
    MSG                             point-05
    ID                              2
    NAME                            Person 2
    SUM                             4
    MSG                             point-05
    ID                              1
    NAME                            Person 1
    SUM                             5
    MSG                             point-06
    ID                              5
    NAME                            Person 5
    S                               1
    MSG                             point-06
    ID                              4
    NAME                            Person 4
    S                               2
    MSG                             point-06
    ID                              3
    NAME                            Person 3
    S                               3
    MSG                             point-06
    ID                              2
    NAME                            Person 2
    S                               4
    MSG                             point-06
    ID                              1
    NAME                            Person 1
    S                               5
    MSG                             point-07
    ID                              1
    NAME                            Person 1
    SUM                             1
    MSG                             point-07
    ID                              2
    NAME                            Person 2
    SUM                             3
    MSG                             point-07
    ID                              3
    NAME                            Person 3
    SUM                             6
    MSG                             point-07
    ID                              4
    NAME                            Person 4
    SUM                             10
    MSG                             point-07
    ID                              5
    NAME                            Person 5
    SUM                             15
    MSG                             point-08
    ID                              1
    NAME                            Person 1
    SUM                             1
    MSG                             point-08
    ID                              2
    NAME                            Person 2
    SUM                             1
    MSG                             point-08
    ID                              3
    NAME                            Person 3
    SUM                             2
    MSG                             point-08
    ID                              4
    NAME                            Person 4
    SUM                             2
    MSG                             point-08
    ID                              5
    NAME                            Person 5
    SUM                             3
    MSG                             point-09
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    AVG                             3.03
    AVG                             2.30
    MSG                             point-09
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    AVG                             4.36
    AVG                             3.30
    MSG                             point-09
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    AVG                             5.70
    AVG                             4.30
    MSG                             point-09
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    AVG                             7.03
    AVG                             5.30
    MSG                             point-09
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    AVG                             8.36
    AVG                             6.30
    MSG                             point-09
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    AVG                             3.03
    AVG                             5.81
    MSG                             point-09
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    AVG                             4.36
    AVG                             5.90
    MSG                             point-09
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    AVG                             5.70
    AVG                             6.33
    MSG                             point-09
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    AVG                             7.03
    AVG                             7.01
    MSG                             point-09
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    AVG                             8.36
    AVG                             7.85
    MSG                             point-09
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    AVG                             3.03
    AVG                             7.44
    MSG                             point-09
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    AVG                             4.36
    AVG                             7.35
    MSG                             point-09
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    AVG                             5.70
    AVG                             7.51
    MSG                             point-09
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    AVG                             7.03
    AVG                             7.86
    MSG                             point-09
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    AVG                             8.36
    AVG                             8.36
    MSG                             point-09
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    AVG                             3.03
    AVG                             <null>
    MSG                             point-10
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    AVG                             3.03
    AVG                             2.30
    MSG                             point-10
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    AVG                             4.36
    AVG                             3.30
    MSG                             point-10
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    AVG                             5.70
    AVG                             4.30
    MSG                             point-10
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    AVG                             7.03
    AVG                             5.30
    MSG                             point-10
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    AVG                             8.36
    AVG                             6.30
    MSG                             point-10
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    AVG                             3.03
    AVG                             5.81
    MSG                             point-10
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    AVG                             4.36
    AVG                             5.90
    MSG                             point-10
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    AVG                             5.70
    AVG                             6.33
    MSG                             point-10
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    AVG                             7.03
    AVG                             7.01
    MSG                             point-10
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    AVG                             8.36
    AVG                             7.85
    MSG                             point-10
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    AVG                             3.03
    AVG                             7.44
    MSG                             point-10
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    AVG                             4.36
    AVG                             7.35
    MSG                             point-10
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    AVG                             5.70
    AVG                             7.51
    MSG                             point-10
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    AVG                             7.03
    AVG                             7.86
    MSG                             point-10
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    AVG                             8.36
    AVG                             8.36
    MSG                             point-10
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    AVG                             3.03
    AVG                             8.36
    MSG                             point-11
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    COUNT                           3
    COUNT                           4
    COUNT                           0
    MSG                             point-11
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    COUNT                           6
    COUNT                           7
    COUNT                           0
    MSG                             point-11
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    COUNT                           9
    COUNT                           10
    COUNT                           0
    MSG                             point-11
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    COUNT                           12
    COUNT                           13
    COUNT                           0
    MSG                             point-11
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    COUNT                           15
    COUNT                           16
    COUNT                           0
    MSG                             point-11
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    COUNT                           3
    COUNT                           4
    COUNT                           0
    MSG                             point-11
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    COUNT                           6
    COUNT                           7
    COUNT                           0
    MSG                             point-11
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    COUNT                           9
    COUNT                           10
    COUNT                           0
    MSG                             point-11
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    COUNT                           12
    COUNT                           13
    COUNT                           0
    MSG                             point-11
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    COUNT                           15
    COUNT                           16
    COUNT                           0
    MSG                             point-11
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    COUNT                           3
    COUNT                           4
    COUNT                           0
    MSG                             point-11
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    COUNT                           6
    COUNT                           7
    COUNT                           0
    MSG                             point-11
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    COUNT                           9
    COUNT                           10
    COUNT                           0
    MSG                             point-11
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    COUNT                           12
    COUNT                           13
    COUNT                           0
    MSG                             point-11
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    COUNT                           15
    COUNT                           16
    COUNT                           0
    MSG                             point-11
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    COUNT                           3
    COUNT                           4
    COUNT                           0
    MSG                             point-12
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    COUNT                           3
    COUNT                           1
    COUNT                           4
    COUNT                           1
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    COUNT                           6
    COUNT                           2
    COUNT                           7
    COUNT                           2
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    COUNT                           9
    COUNT                           3
    COUNT                           10
    COUNT                           3
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    COUNT                           12
    COUNT                           4
    COUNT                           13
    COUNT                           4
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    COUNT                           15
    COUNT                           5
    COUNT                           16
    COUNT                           5
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    COUNT                           3
    COUNT                           6
    COUNT                           4
    COUNT                           6
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    COUNT                           6
    COUNT                           7
    COUNT                           7
    COUNT                           7
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    COUNT                           9
    COUNT                           8
    COUNT                           10
    COUNT                           8
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    COUNT                           12
    COUNT                           9
    COUNT                           13
    COUNT                           9
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    COUNT                           15
    COUNT                           10
    COUNT                           16
    COUNT                           10
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    COUNT                           3
    COUNT                           11
    COUNT                           4
    COUNT                           11
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    COUNT                           6
    COUNT                           12
    COUNT                           7
    COUNT                           12
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    COUNT                           9
    COUNT                           13
    COUNT                           10
    COUNT                           13
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    COUNT                           12
    COUNT                           14
    COUNT                           13
    COUNT                           14
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    COUNT                           15
    COUNT                           15
    COUNT                           16
    COUNT                           15
    COUNT                           0
    COUNT                           0
    MSG                             point-12
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    COUNT                           3
    COUNT                           15
    COUNT                           4
    COUNT                           16
    COUNT                           0
    COUNT                           0
    MSG                             point-13
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    SUM                             2.30
    SUM                             2.30
    COUNT                           4
    COUNT                           1
    SUM                             1
    SUM                             1
    MSG                             point-13
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    SUM                             4.30
    SUM                             4.30
    COUNT                           3
    COUNT                           1
    SUM                             2
    SUM                             2
    MSG                             point-13
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    SUM                             6.30
    SUM                             6.30
    COUNT                           3
    COUNT                           1
    SUM                             3
    SUM                             3
    MSG                             point-13
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    SUM                             8.30
    SUM                             8.30
    COUNT                           3
    COUNT                           1
    SUM                             4
    SUM                             4
    MSG                             point-13
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    SUM                             10.30
    SUM                             10.30
    COUNT                           3
    COUNT                           1
    SUM                             5
    SUM                             5
    MSG                             point-13
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    SUM                             3.40
    SUM                             3.40
    COUNT                           4
    COUNT                           2
    SUM                             6
    SUM                             6
    MSG                             point-13
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    SUM                             6.40
    SUM                             6.40
    COUNT                           3
    COUNT                           2
    SUM                             7
    SUM                             7
    MSG                             point-13
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    SUM                             9.40
    SUM                             9.40
    COUNT                           3
    COUNT                           2
    SUM                             8
    SUM                             8
    MSG                             point-13
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    SUM                             12.40
    SUM                             12.40
    COUNT                           3
    COUNT                           2
    SUM                             9
    SUM                             9
    MSG                             point-13
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    SUM                             15.40
    SUM                             15.40
    COUNT                           3
    COUNT                           2
    SUM                             10
    SUM                             10
    MSG                             point-13
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    SUM                             3.40
    SUM                             3.40
    COUNT                           4
    COUNT                           3
    SUM                             11
    SUM                             11
    MSG                             point-13
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    SUM                             6.40
    SUM                             6.40
    COUNT                           3
    COUNT                           3
    SUM                             12
    SUM                             12
    MSG                             point-13
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    SUM                             9.40
    SUM                             9.40
    COUNT                           3
    COUNT                           3
    SUM                             13
    SUM                             13
    MSG                             point-13
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    SUM                             12.40
    SUM                             12.40
    COUNT                           3
    COUNT                           3
    SUM                             14
    SUM                             14
    MSG                             point-13
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    SUM                             15.40
    SUM                             15.40
    COUNT                           3
    COUNT                           3
    SUM                             15
    SUM                             15
    MSG                             point-13
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    SUM                             <null>
    SUM                             <null>
    COUNT                           4
    COUNT                           4
    SUM                             16
    SUM                             16
    MSG                             point-14
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    SUM                             31.50
    SUM                             15
    SUM                             125.50
    SUM                             120
    SUM                             15.10
    SUM                             20
    MSG                             point-14
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    SUM                             31.50
    SUM                             15
    SUM                             125.50
    SUM                             120
    SUM                             23.10
    SUM                             23
    MSG                             point-14
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    SUM                             31.50
    SUM                             15
    SUM                             125.50
    SUM                             120
    SUM                             31.10
    SUM                             26
    MSG                             point-14
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    SUM                             31.50
    SUM                             15
    SUM                             125.50
    SUM                             120
    SUM                             39.10
    SUM                             29
    MSG                             point-14
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    SUM                             31.50
    SUM                             15
    SUM                             125.50
    SUM                             120
    SUM                             10.30
    SUM                             5
    MSG                             point-14
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    SUM                             47.00
    SUM                             40
    SUM                             125.50
    SUM                             120
    SUM                             6.80
    SUM                             17
    MSG                             point-14
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    SUM                             47.00
    SUM                             40
    SUM                             125.50
    SUM                             120
    SUM                             15.10
    SUM                             20
    MSG                             point-14
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    SUM                             47.00
    SUM                             40
    SUM                             125.50
    SUM                             120
    SUM                             23.10
    SUM                             23
    MSG                             point-14
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    SUM                             47.00
    SUM                             40
    SUM                             125.50
    SUM                             120
    SUM                             31.10
    SUM                             26
    MSG                             point-14
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    SUM                             47.00
    SUM                             40
    SUM                             125.50
    SUM                             120
    SUM                             39.10
    SUM                             29
    MSG                             point-14
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    SUM                             47.00
    SUM                             65
    SUM                             125.50
    SUM                             120
    SUM                             6.80
    SUM                             17
    MSG                             point-14
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    SUM                             47.00
    SUM                             65
    SUM                             125.50
    SUM                             120
    SUM                             15.10
    SUM                             20
    MSG                             point-14
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    SUM                             47.00
    SUM                             65
    SUM                             125.50
    SUM                             120
    SUM                             23.10
    SUM                             23
    MSG                             point-14
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    SUM                             47.00
    SUM                             65
    SUM                             125.50
    SUM                             120
    SUM                             31.10
    SUM                             26
    MSG                             point-14
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    SUM                             47.00
    SUM                             65
    SUM                             125.50
    SUM                             120
    SUM                             39.10
    SUM                             29
    MSG                             point-14
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    SUM                             <null>
    SUM                             16
    SUM                             <null>
    SUM                             16
    SUM                             <null>
    SUM                             16
    MSG                             point-15
    ID                              1
    PERSON                          1
    DAT                             2010-01-03
    VAL                             2.30
    MIN                             1
    MAX                             5
    MIN                             2.30
    MAX                             10.30
    MSG                             point-15
    ID                              2
    PERSON                          2
    DAT                             2010-01-04
    VAL                             4.30
    MIN                             1
    MAX                             5
    MIN                             2.30
    MAX                             10.30
    MSG                             point-15
    ID                              3
    PERSON                          3
    DAT                             2010-01-05
    VAL                             6.30
    MIN                             1
    MAX                             5
    MIN                             2.30
    MAX                             10.30
    MSG                             point-15
    ID                              4
    PERSON                          4
    DAT                             2010-01-06
    VAL                             8.30
    MIN                             1
    MAX                             5
    MIN                             2.30
    MAX                             10.30
    MSG                             point-15
    ID                              5
    PERSON                          5
    DAT                             2010-01-07
    VAL                             10.30
    MIN                             1
    MAX                             5
    MIN                             2.30
    MAX                             10.30
    MSG                             point-15
    ID                              6
    PERSON                          1
    DAT                             2010-02-02
    VAL                             3.40
    MIN                             6
    MAX                             10
    MIN                             3.40
    MAX                             15.40
    MSG                             point-15
    ID                              7
    PERSON                          2
    DAT                             2010-02-03
    VAL                             6.40
    MIN                             6
    MAX                             10
    MIN                             3.40
    MAX                             15.40
    MSG                             point-15
    ID                              8
    PERSON                          3
    DAT                             2010-02-04
    VAL                             9.40
    MIN                             6
    MAX                             10
    MIN                             3.40
    MAX                             15.40
    MSG                             point-15
    ID                              9
    PERSON                          4
    DAT                             2010-02-05
    VAL                             12.40
    MIN                             6
    MAX                             10
    MIN                             3.40
    MAX                             15.40
    MSG                             point-15
    ID                              10
    PERSON                          5
    DAT                             2010-02-06
    VAL                             15.40
    MIN                             6
    MAX                             10
    MIN                             3.40
    MAX                             15.40
    MSG                             point-15
    ID                              11
    PERSON                          1
    DAT                             2010-03-02
    VAL                             3.40
    MIN                             11
    MAX                             15
    MIN                             3.40
    MAX                             15.40
    MSG                             point-15
    ID                              12
    PERSON                          2
    DAT                             2010-03-03
    VAL                             6.40
    MIN                             11
    MAX                             15
    MIN                             3.40
    MAX                             15.40
    MSG                             point-15
    ID                              13
    PERSON                          3
    DAT                             2010-03-04
    VAL                             9.40
    MIN                             11
    MAX                             15
    MIN                             3.40
    MAX                             15.40
    MSG                             point-15
    ID                              14
    PERSON                          4
    DAT                             2010-03-05
    VAL                             12.40
    MIN                             11
    MAX                             15
    MIN                             3.40
    MAX                             15.40
    MSG                             point-15
    ID                              15
    PERSON                          5
    DAT                             2010-03-06
    VAL                             15.40
    MIN                             11
    MAX                             15
    MIN                             3.40
    MAX                             15.40
    MSG                             point-15
    ID                              16
    PERSON                          1
    DAT                             <null>
    VAL                             <null>
    MIN                             16
    MAX                             16
    MIN                             <null>
    MAX                             <null>
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    sql_init = (act.files_dir / 'gtcs-window-func.sql').read_text()

    sql_addi= \
    """
      set list on;

      select
          'point-01' as msg,
          p.*,
          sum(1) over (order by id)
        from persons p
        order by id;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-02' as msg,
          p.*,
          sum(1) over (order by id desc)
        from persons p
        order by id;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-03' as msg,
          p.*,
          sum(1) over (order by id)
        from persons p
        order by id;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-04' as msg,
          p.*,
          sum(1) over (order by id)
        from persons p
        order by id desc;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-05' as msg,
          p.*,
          sum(1) over (order by id desc)
        from persons p
        order by id desc;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-06' as msg,
          p.*,
          sum(1) over (order by id desc) s
        from persons p
        order by s;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-07' as msg,
          p.*,
          sum(id) over (order by id)
        from persons p;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-08' as msg,
          p.*,
          sum(mod(id, 2)) over (order by id)
        from persons p;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-09' as msg,
          e.*,
          avg(val) over (order by person nulls first),
          avg(val) over (order by dat nulls first)
        from entries e
        order by id;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-10' as msg,
          e.*,
          avg(val) over (order by person nulls last),
          avg(val) over (order by dat nulls last)
        from entries e
        order by id;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-11' as msg,
          e.*,
          count(val) over (order by person),
          count(*) over (order by person),
          count(null) over (order by person)
        from entries e
        order by id;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-12' as msg,
          e.*,
          count(val) over (order by person),
          count(val) over (order by id),
          count(*) over (order by person),
          count(*) over (order by id),
          count(null) over (order by person),
          count(null) over (order by id)
        from entries e
        order by id;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-13' as msg,
          e.*,
          sum(val) over (partition by id order by person),
          sum(val) over (partition by id order by id),
          count(*) over (partition by person order by person),
          count(*) over (partition by person order by id),
          sum(id) over (partition by dat order by person),
          sum(id) over (partition by dat order by id)
        from entries e
        order by id;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-14' as msg,
          e.*,
          sum(val) over (partition by extract(month from dat)),
          sum(id) over (partition by extract(month from dat)),
          sum(val) over (partition by extract(year from dat)),
          sum(id) over (partition by extract(year from dat)),
          sum(val) over (partition by extract(day from dat)),
          sum(id) over (partition by extract(day from dat))
        from entries e
        order by id;

      --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

      select
          'point-15' as msg,
          e.*,
          min(id) over (partition by extract(month from dat)),
          max(id) over (partition by extract(month from dat)),
          min(val) over (partition by extract(month from dat)),
          max(val) over (partition by extract(month from dat))
        from entries e
        order by id;
    """

    act.expected_stdout = test_expected_stdout

    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ) )

    assert act.clean_stdout == act.clean_expected_stdout
