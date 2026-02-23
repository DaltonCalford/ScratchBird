#coding:utf-8

"""
ID:          gtcs.window-func-05
TITLE:       Set of miscelaneous tests for verification of windowed functions
DESCRIPTION:
  Statements from this test are added to initial SQL which is stored in: /files/gtcs-window-func.sql
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/FB_SQL_WINDOW_FUNC_05.script

NOTES:
    This test used functionality that exists in FB 4.0+.

FBTEST:      functional.gtcs.window_func_05
"""

import os
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

test_expected_stdout = """
    MSG                             point-01
    N1                              1
    N2                              1
    X1                              1
    X2                              10111
    X3                              1
    X4                              <null>
    X5                              <null>
    X6                              10111
    MSG                             point-01
    N1                              1
    N2                              10
    X1                              11
    X2                              10110
    X3                              10
    X4                              <null>
    X5                              <null>
    X6                              10110
    MSG                             point-01
    N1                              2
    N2                              20
    X1                              20
    X2                              20
    X3                              20
    X4                              <null>
    X5                              <null>
    X6                              20
    MSG                             point-01
    N1                              <null>
    N2                              50
    X1                              50
    X2                              101110
    X3                              50
    X4                              <null>
    X5                              <null>
    X6                              101110
    MSG                             point-01
    N1                              <null>
    N2                              60
    X1                              110
    X2                              101060
    X3                              60
    X4                              <null>
    X5                              <null>
    X6                              101060
    MSG                             point-01
    N1                              1
    N2                              100
    X1                              111
    X2                              10100
    X3                              100
    X4                              <null>
    X5                              <null>
    X6                              10100
    MSG                             point-01
    N1                              3
    N2                              300
    X1                              300
    X2                              300
    X3                              300
    X4                              <null>
    X5                              <null>
    X6                              300
    MSG                             point-01
    N1                              5
    N2                              500
    X1                              500
    X2                              500
    X3                              500
    X4                              <null>
    X5                              <null>
    X6                              500
    MSG                             point-01
    N1                              <null>
    N2                              1000
    X1                              1110
    X2                              101000
    X3                              1000
    X4                              <null>
    X5                              <null>
    X6                              101000
    MSG                             point-01
    N1                              1
    N2                              10000
    X1                              10111
    X2                              10000
    X3                              10000
    X4                              <null>
    X5                              <null>
    X6                              10000
    MSG                             point-01
    N1                              <null>
    N2                              100000
    X1                              101110
    X2                              100000
    X3                              100000
    X4                              <null>
    X5                              <null>
    X6                              100000
    MSG                             point-02
    N1                              1
    N2                              1
    X1                              1
    X2                              10111
    X3                              1
    X4                              10100
    X5                              <null>
    X6                              10111
    MSG                             point-02
    N1                              1
    N2                              10
    X1                              11
    X2                              10110
    X3                              10
    X4                              10000
    X5                              <null>
    X6                              10111
    MSG                             point-02
    N1                              2
    N2                              20
    X1                              20
    X2                              20
    X3                              20
    X4                              <null>
    X5                              <null>
    X6                              20
    MSG                             point-02
    N1                              <null>
    N2                              50
    X1                              50
    X2                              101110
    X3                              50
    X4                              101000
    X5                              <null>
    X6                              101110
    MSG                             point-02
    N1                              <null>
    N2                              60
    X1                              110
    X2                              101060
    X3                              60
    X4                              100000
    X5                              <null>
    X6                              101110
    MSG                             point-02
    N1                              1
    N2                              100
    X1                              111
    X2                              10100
    X3                              100
    X4                              <null>
    X5                              1
    X6                              10111
    MSG                             point-02
    N1                              3
    N2                              300
    X1                              300
    X2                              300
    X3                              300
    X4                              <null>
    X5                              <null>
    X6                              300
    MSG                             point-02
    N1                              5
    N2                              500
    X1                              500
    X2                              500
    X3                              500
    X4                              <null>
    X5                              <null>
    X6                              500
    MSG                             point-02
    N1                              <null>
    N2                              1000
    X1                              1110
    X2                              101000
    X3                              1000
    X4                              <null>
    X5                              50
    X6                              101110
    MSG                             point-02
    N1                              1
    N2                              10000
    X1                              10111
    X2                              10000
    X3                              10000
    X4                              <null>
    X5                              11
    X6                              10111
    MSG                             point-02
    N1                              <null>
    N2                              100000
    X1                              101110
    X2                              100000
    X3                              100000
    X4                              <null>
    X5                              110
    X6                              101110
    MSG                             point-03
    N1                              <null>
    N2                              50
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-03
    N1                              <null>
    N2                              60
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-03
    N1                              <null>
    N2                              1000
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-03
    N1                              <null>
    N2                              100000
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-03
    N1                              1
    N2                              1
    X1                              111221
    X2                              10931
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              10931
    MSG                             point-03
    N1                              1
    N2                              10
    X1                              111221
    X2                              10931
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              10931
    MSG                             point-03
    N1                              1
    N2                              100
    X1                              111221
    X2                              10931
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              10931
    MSG                             point-03
    N1                              1
    N2                              10000
    X1                              111221
    X2                              10931
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              10931
    MSG                             point-03
    N1                              2
    N2                              20
    X1                              111241
    X2                              820
    X3                              20
    X4                              500
    X5                              <null>
    X6                              10931
    MSG                             point-03
    N1                              3
    N2                              300
    X1                              111541
    X2                              800
    X3                              300
    X4                              500
    X5                              10111
    X6                              10931
    MSG                             point-03
    N1                              5
    N2                              500
    X1                              112041
    X2                              500
    X3                              500
    X4                              <null>
    X5                              320
    X6                              820
    MSG                             point-04
    N1                              <null>
    N2                              50
    X1                              50
    X2                              112041
    X3                              50
    X4                              101000
    X5                              <null>
    X6                              112041
    MSG                             point-04
    N1                              <null>
    N2                              60
    X1                              110
    X2                              111991
    X3                              60
    X4                              100001
    X5                              <null>
    X6                              112041
    MSG                             point-04
    N1                              <null>
    N2                              1000
    X1                              1110
    X2                              111931
    X3                              1000
    X4                              11
    X5                              50
    X6                              112041
    MSG                             point-04
    N1                              <null>
    N2                              100000
    X1                              101110
    X2                              110931
    X3                              100000
    X4                              110
    X5                              110
    X6                              112041
    MSG                             point-04
    N1                              1
    N2                              1
    X1                              101111
    X2                              10931
    X3                              1
    X4                              10100
    X5                              1060
    X6                              111991
    MSG                             point-04
    N1                              1
    N2                              10
    X1                              101121
    X2                              10930
    X3                              10
    X4                              10020
    X5                              101000
    X6                              111931
    MSG                             point-04
    N1                              1
    N2                              100
    X1                              101221
    X2                              10920
    X3                              100
    X4                              320
    X5                              100001
    X6                              110931
    MSG                             point-04
    N1                              1
    N2                              10000
    X1                              111221
    X2                              10820
    X3                              10000
    X4                              800
    X5                              11
    X6                              10931
    MSG                             point-04
    N1                              2
    N2                              20
    X1                              111241
    X2                              820
    X3                              20
    X4                              500
    X5                              110
    X6                              10930
    MSG                             point-04
    N1                              3
    N2                              300
    X1                              111541
    X2                              800
    X3                              300
    X4                              <null>
    X5                              10100
    X6                              10920
    MSG                             point-04
    N1                              5
    N2                              500
    X1                              112041
    X2                              500
    X3                              500
    X4                              <null>
    X5                              10020
    X6                              10820
    MSG                             point-05
    N1                              <null>
    N2                              50
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-05
    N1                              <null>
    N2                              60
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-05
    N1                              <null>
    N2                              1000
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-05
    N1                              <null>
    N2                              100000
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-05
    N1                              1
    N2                              1
    X1                              111221
    X2                              10931
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              10931
    MSG                             point-05
    N1                              1
    N2                              10
    X1                              111221
    X2                              10931
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              10931
    MSG                             point-05
    N1                              1
    N2                              100
    X1                              111221
    X2                              10931
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              10931
    MSG                             point-05
    N1                              1
    N2                              10000
    X1                              111221
    X2                              10931
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              10931
    MSG                             point-05
    N1                              2
    N2                              20
    X1                              111241
    X2                              820
    X3                              20
    X4                              500
    X5                              <null>
    X6                              10931
    MSG                             point-05
    N1                              3
    N2                              300
    X1                              111541
    X2                              800
    X3                              300
    X4                              500
    X5                              10111
    X6                              10931
    MSG                             point-05
    N1                              5
    N2                              500
    X1                              112041
    X2                              500
    X3                              500
    X4                              <null>
    X5                              320
    X6                              820
    MSG                             point-06
    N1                              <null>
    N2                              50
    X1                              50
    X2                              112041
    X3                              50
    X4                              101000
    X5                              <null>
    X6                              112041
    MSG                             point-06
    N1                              <null>
    N2                              60
    X1                              110
    X2                              111991
    X3                              60
    X4                              100001
    X5                              <null>
    X6                              112041
    MSG                             point-06
    N1                              <null>
    N2                              1000
    X1                              1110
    X2                              111931
    X3                              1000
    X4                              11
    X5                              50
    X6                              112041
    MSG                             point-06
    N1                              <null>
    N2                              100000
    X1                              101110
    X2                              110931
    X3                              100000
    X4                              110
    X5                              110
    X6                              112041
    MSG                             point-06
    N1                              1
    N2                              1
    X1                              101111
    X2                              10931
    X3                              1
    X4                              10100
    X5                              1060
    X6                              111991
    MSG                             point-06
    N1                              1
    N2                              10
    X1                              101121
    X2                              10930
    X3                              10
    X4                              10020
    X5                              101000
    X6                              111931
    MSG                             point-06
    N1                              1
    N2                              100
    X1                              101221
    X2                              10920
    X3                              100
    X4                              320
    X5                              100001
    X6                              110931
    MSG                             point-06
    N1                              1
    N2                              10000
    X1                              111221
    X2                              10820
    X3                              10000
    X4                              800
    X5                              11
    X6                              10931
    MSG                             point-06
    N1                              2
    N2                              20
    X1                              111241
    X2                              820
    X3                              20
    X4                              500
    X5                              110
    X6                              10930
    MSG                             point-06
    N1                              3
    N2                              300
    X1                              111541
    X2                              800
    X3                              300
    X4                              <null>
    X5                              10100
    X6                              10920
    MSG                             point-06
    N1                              5
    N2                              500
    X1                              112041
    X2                              500
    X3                              500
    X4                              <null>
    X5                              10020
    X6                              10820
    MSG                             point-07
    N1                              1
    N2                              1
    X1                              10111
    X2                              112041
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              112041
    MSG                             point-07
    N1                              1
    N2                              10
    X1                              10111
    X2                              112041
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              112041
    MSG                             point-07
    N1                              1
    N2                              100
    X1                              10111
    X2                              112041
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              112041
    MSG                             point-07
    N1                              1
    N2                              10000
    X1                              10111
    X2                              112041
    X3                              10111
    X4                              300
    X5                              <null>
    X6                              112041
    MSG                             point-07
    N1                              2
    N2                              20
    X1                              10131
    X2                              101930
    X3                              20
    X4                              500
    X5                              <null>
    X6                              112041
    MSG                             point-07
    N1                              3
    N2                              300
    X1                              10431
    X2                              101910
    X3                              300
    X4                              500
    X5                              10111
    X6                              112041
    MSG                             point-07
    N1                              5
    N2                              500
    X1                              10931
    X2                              101610
    X3                              500
    X4                              <null>
    X5                              320
    X6                              101930
    MSG                             point-07
    N1                              <null>
    N2                              50
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-07
    N1                              <null>
    N2                              60
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-07
    N1                              <null>
    N2                              1000
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-07
    N1                              <null>
    N2                              100000
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-08
    N1                              1
    N2                              1
    X1                              1
    X2                              112041
    X3                              1
    X4                              10100
    X5                              <null>
    X6                              112041
    MSG                             point-08
    N1                              1
    N2                              10
    X1                              11
    X2                              112040
    X3                              10
    X4                              10020
    X5                              <null>
    X6                              112041
    MSG                             point-08
    N1                              1
    N2                              100
    X1                              111
    X2                              112030
    X3                              100
    X4                              320
    X5                              1
    X6                              112041
    MSG                             point-08
    N1                              1
    N2                              10000
    X1                              10111
    X2                              111930
    X3                              10000
    X4                              800
    X5                              11
    X6                              112041
    MSG                             point-08
    N1                              2
    N2                              20
    X1                              10131
    X2                              101930
    X3                              20
    X4                              550
    X5                              110
    X6                              112040
    MSG                             point-08
    N1                              3
    N2                              300
    X1                              10431
    X2                              101910
    X3                              300
    X4                              110
    X5                              10100
    X6                              112030
    MSG                             point-08
    N1                              5
    N2                              500
    X1                              10931
    X2                              101610
    X3                              500
    X4                              1060
    X5                              10020
    X6                              111930
    MSG                             point-08
    N1                              <null>
    N2                              50
    X1                              10981
    X2                              101110
    X3                              50
    X4                              101000
    X5                              320
    X6                              101930
    MSG                             point-08
    N1                              <null>
    N2                              60
    X1                              11041
    X2                              101060
    X3                              60
    X4                              100000
    X5                              800
    X6                              101910
    MSG                             point-08
    N1                              <null>
    N2                              1000
    X1                              12041
    X2                              101000
    X3                              1000
    X4                              <null>
    X5                              550
    X6                              101610
    MSG                             point-08
    N1                              <null>
    N2                              100000
    X1                              112041
    X2                              100000
    X3                              100000
    X4                              <null>
    X5                              110
    X6                              101110
    MSG                             point-09
    N1                              5
    N2                              500
    X1                              500
    X2                              112041
    X3                              500
    X4                              320
    X5                              <null>
    X6                              112041
    MSG                             point-09
    N1                              3
    N2                              300
    X1                              800
    X2                              111541
    X3                              300
    X4                              10111
    X5                              500
    X6                              112041
    MSG                             point-09
    N1                              2
    N2                              20
    X1                              820
    X2                              111241
    X3                              20
    X4                              <null>
    X5                              500
    X6                              112041
    MSG                             point-09
    N1                              1
    N2                              1
    X1                              10931
    X2                              111221
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              111541
    MSG                             point-09
    N1                              1
    N2                              10
    X1                              10931
    X2                              111221
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              111541
    MSG                             point-09
    N1                              1
    N2                              100
    X1                              10931
    X2                              111221
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              111541
    MSG                             point-09
    N1                              1
    N2                              10000
    X1                              10931
    X2                              111221
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              111541
    MSG                             point-09
    N1                              <null>
    N2                              50
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-09
    N1                              <null>
    N2                              60
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-09
    N1                              <null>
    N2                              1000
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-09
    N1                              <null>
    N2                              100000
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-10
    N1                              5
    N2                              500
    X1                              500
    X2                              112041
    X3                              500
    X4                              21
    X5                              <null>
    X6                              112041
    MSG                             point-10
    N1                              3
    N2                              300
    X1                              800
    X2                              111541
    X3                              300
    X4                              11
    X5                              <null>
    X6                              112041
    MSG                             point-10
    N1                              2
    N2                              20
    X1                              820
    X2                              111241
    X3                              20
    X4                              110
    X5                              500
    X6                              112041
    MSG                             point-10
    N1                              1
    N2                              1
    X1                              821
    X2                              111221
    X3                              1
    X4                              10100
    X5                              800
    X6                              112041
    MSG                             point-10
    N1                              1
    N2                              10
    X1                              831
    X2                              111220
    X3                              10
    X4                              10050
    X5                              320
    X6                              111541
    MSG                             point-10
    N1                              1
    N2                              100
    X1                              931
    X2                              111210
    X3                              100
    X4                              110
    X5                              21
    X6                              111241
    MSG                             point-10
    N1                              1
    N2                              10000
    X1                              10931
    X2                              111110
    X3                              10000
    X4                              1060
    X5                              11
    X6                              111221
    MSG                             point-10
    N1                              <null>
    N2                              50
    X1                              10981
    X2                              101110
    X3                              50
    X4                              101000
    X5                              110
    X6                              111220
    MSG                             point-10
    N1                              <null>
    N2                              60
    X1                              11041
    X2                              101060
    X3                              60
    X4                              100000
    X5                              10100
    X6                              111210
    MSG                             point-10
    N1                              <null>
    N2                              1000
    X1                              12041
    X2                              101000
    X3                              1000
    X4                              <null>
    X5                              10050
    X6                              111110
    MSG                             point-10
    N1                              <null>
    N2                              100000
    X1                              112041
    X2                              100000
    X3                              100000
    X4                              <null>
    X5                              110
    X6                              101110
    MSG                             point-11
    N1                              <null>
    N2                              50
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-11
    N1                              <null>
    N2                              60
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-11
    N1                              <null>
    N2                              1000
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-11
    N1                              <null>
    N2                              100000
    X1                              101110
    X2                              112041
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              112041
    MSG                             point-11
    N1                              5
    N2                              500
    X1                              101610
    X2                              10931
    X3                              500
    X4                              320
    X5                              <null>
    X6                              10931
    MSG                             point-11
    N1                              3
    N2                              300
    X1                              101910
    X2                              10431
    X3                              300
    X4                              10111
    X5                              500
    X6                              10931
    MSG                             point-11
    N1                              2
    N2                              20
    X1                              101930
    X2                              10131
    X3                              20
    X4                              <null>
    X5                              500
    X6                              10931
    MSG                             point-11
    N1                              1
    N2                              1
    X1                              112041
    X2                              10111
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              10431
    MSG                             point-11
    N1                              1
    N2                              10
    X1                              112041
    X2                              10111
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              10431
    MSG                             point-11
    N1                              1
    N2                              100
    X1                              112041
    X2                              10111
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              10431
    MSG                             point-11
    N1                              1
    N2                              10000
    X1                              112041
    X2                              10111
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              10431
    MSG                             point-12
    N1                              <null>
    N2                              50
    X1                              50
    X2                              112041
    X3                              50
    X4                              101000
    X5                              <null>
    X6                              112041
    MSG                             point-12
    N1                              <null>
    N2                              60
    X1                              110
    X2                              111991
    X3                              60
    X4                              100500
    X5                              <null>
    X6                              112041
    MSG                             point-12
    N1                              <null>
    N2                              1000
    X1                              1110
    X2                              111931
    X3                              1000
    X4                              800
    X5                              50
    X6                              112041
    MSG                             point-12
    N1                              <null>
    N2                              100000
    X1                              101110
    X2                              110931
    X3                              100000
    X4                              320
    X5                              110
    X6                              112041
    MSG                             point-12
    N1                              5
    N2                              500
    X1                              101610
    X2                              10931
    X3                              500
    X4                              21
    X5                              1060
    X6                              111991
    MSG                             point-12
    N1                              3
    N2                              300
    X1                              101910
    X2                              10431
    X3                              300
    X4                              11
    X5                              101000
    X6                              111931
    MSG                             point-12
    N1                              2
    N2                              20
    X1                              101930
    X2                              10131
    X3                              20
    X4                              110
    X5                              100500
    X6                              110931
    MSG                             point-12
    N1                              1
    N2                              1
    X1                              101931
    X2                              10111
    X3                              1
    X4                              10100
    X5                              800
    X6                              10931
    MSG                             point-12
    N1                              1
    N2                              10
    X1                              101941
    X2                              10110
    X3                              10
    X4                              10000
    X5                              320
    X6                              10431
    MSG                             point-12
    N1                              1
    N2                              100
    X1                              102041
    X2                              10100
    X3                              100
    X4                              <null>
    X5                              21
    X6                              10131
    MSG                             point-12
    N1                              1
    N2                              10000
    X1                              112041
    X2                              10000
    X3                              10000
    X4                              <null>
    X5                              11
    X6                              10111
    MSG                             point-13
    N1                              5
    N2                              500
    X1                              500
    X2                              112041
    X3                              500
    X4                              320
    X5                              <null>
    X6                              112041
    MSG                             point-13
    N1                              3
    N2                              300
    X1                              800
    X2                              111541
    X3                              300
    X4                              10111
    X5                              500
    X6                              112041
    MSG                             point-13
    N1                              2
    N2                              20
    X1                              820
    X2                              111241
    X3                              20
    X4                              <null>
    X5                              500
    X6                              112041
    MSG                             point-13
    N1                              1
    N2                              1
    X1                              10931
    X2                              111221
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              111541
    MSG                             point-13
    N1                              1
    N2                              10
    X1                              10931
    X2                              111221
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              111541
    MSG                             point-13
    N1                              1
    N2                              100
    X1                              10931
    X2                              111221
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              111541
    MSG                             point-13
    N1                              1
    N2                              10000
    X1                              10931
    X2                              111221
    X3                              10111
    X4                              <null>
    X5                              300
    X6                              111541
    MSG                             point-13
    N1                              <null>
    N2                              50
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-13
    N1                              <null>
    N2                              60
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-13
    N1                              <null>
    N2                              1000
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-13
    N1                              <null>
    N2                              100000
    X1                              112041
    X2                              101110
    X3                              101110
    X4                              101110
    X5                              101110
    X6                              101110
    MSG                             point-14
    N1                              5
    N2                              500
    X1                              500
    X2                              112041
    X3                              500
    X4                              21
    X5                              <null>
    X6                              112041
    MSG                             point-14
    N1                              3
    N2                              300
    X1                              800
    X2                              111541
    X3                              300
    X4                              11
    X5                              <null>
    X6                              112041
    MSG                             point-14
    N1                              2
    N2                              20
    X1                              820
    X2                              111241
    X3                              20
    X4                              110
    X5                              500
    X6                              112041
    MSG                             point-14
    N1                              1
    N2                              1
    X1                              821
    X2                              111221
    X3                              1
    X4                              10100
    X5                              800
    X6                              112041
    MSG                             point-14
    N1                              1
    N2                              10
    X1                              831
    X2                              111220
    X3                              10
    X4                              10050
    X5                              320
    X6                              111541
    MSG                             point-14
    N1                              1
    N2                              100
    X1                              931
    X2                              111210
    X3                              100
    X4                              110
    X5                              21
    X6                              111241
    MSG                             point-14
    N1                              1
    N2                              10000
    X1                              10931
    X2                              111110
    X3                              10000
    X4                              1060
    X5                              11
    X6                              111221
    MSG                             point-14
    N1                              <null>
    N2                              50
    X1                              10981
    X2                              101110
    X3                              50
    X4                              101000
    X5                              110
    X6                              111220
    MSG                             point-14
    N1                              <null>
    N2                              60
    X1                              11041
    X2                              101060
    X3                              60
    X4                              100000
    X5                              10100
    X6                              111210
    MSG                             point-14
    N1                              <null>
    N2                              1000
    X1                              12041
    X2                              101000
    X3                              1000
    X4                              <null>
    X5                              10050
    X6                              111110
    MSG                             point-14
    N1                              <null>
    N2                              100000
    X1                              112041
    X2                              100000
    X3                              100000
    X4                              <null>
    X5                              110
    X6                              101110
    MSG                             point-15
    N1                              <null>
    N2                              50
    X1                              4
    X2                              101110
    X3                              4
    X4                              101110
    MSG                             point-15
    N1                              <null>
    N2                              60
    X1                              4
    X2                              101110
    X3                              4
    X4                              101110
    MSG                             point-15
    N1                              <null>
    N2                              1000
    X1                              4
    X2                              101110
    X3                              4
    X4                              101110
    MSG                             point-15
    N1                              <null>
    N2                              100000
    X1                              4
    X2                              101110
    X3                              4
    X4                              101110
    MSG                             point-15
    N1                              1
    N2                              1
    X1                              0
    X2                              <null>
    X3                              1
    X4                              20
    MSG                             point-15
    N1                              1
    N2                              10
    X1                              0
    X2                              <null>
    X3                              1
    X4                              20
    MSG                             point-15
    N1                              1
    N2                              100
    X1                              0
    X2                              <null>
    X3                              1
    X4                              20
    MSG                             point-15
    N1                              1
    N2                              10000
    X1                              0
    X2                              <null>
    X3                              1
    X4                              20
    MSG                             point-15
    N1                              2
    N2                              20
    X1                              0
    X2                              <null>
    X3                              0
    X4                              <null>
    MSG                             point-15
    N1                              3
    N2                              300
    X1                              0
    X2                              <null>
    X3                              0
    X4                              <null>
    MSG                             point-15
    N1                              5
    N2                              500
    X1                              0
    X2                              <null>
    X3                              0
    X4                              <null>
    MSG                             point-16
    N1                              <null>
    N2                              50
    X1                              1
    X2                              50
    X3                              1
    X4                              50
    MSG                             point-16
    N1                              <null>
    N2                              60
    X1                              1
    X2                              60
    X3                              1
    X4                              60
    MSG                             point-16
    N1                              <null>
    N2                              1000
    X1                              1
    X2                              1000
    X3                              1
    X4                              1000
    MSG                             point-16
    N1                              <null>
    N2                              100000
    X1                              1
    X2                              100000
    X3                              1
    X4                              100000
    MSG                             point-16
    N1                              1
    N2                              1
    X1                              1
    X2                              100000
    X3                              1
    X4                              10
    MSG                             point-16
    N1                              1
    N2                              10
    X1                              1
    X2                              1
    X3                              1
    X4                              100
    MSG                             point-16
    N1                              1
    N2                              100
    X1                              1
    X2                              10
    X3                              1
    X4                              10000
    MSG                             point-16
    N1                              1
    N2                              10000
    X1                              1
    X2                              100
    X3                              1
    X4                              20
    MSG                             point-16
    N1                              2
    N2                              20
    X1                              1
    X2                              100
    X3                              1
    X4                              500
    MSG                             point-16
    N1                              3
    N2                              300
    X1                              1
    X2                              100
    X3                              0
    X4                              <null>
    MSG                             point-16
    N1                              5
    N2                              500
    X1                              1
    X2                              10
    X3                              0
    X4                              <null>
    MSG                             point-17
    N1                              1
    N2                              1
    X1                              1
    X2                              10111
    X3                              1
    X4                              11
    X5                              10111
    MSG                             point-17
    N1                              1
    N2                              10
    X1                              11
    X2                              10110
    X3                              10
    X4                              11
    X5                              10110
    MSG                             point-17
    N1                              2
    N2                              20
    X1                              20
    X2                              20
    X3                              20
    X4                              20
    X5                              20
    MSG                             point-17
    N1                              <null>
    N2                              50
    X1                              50
    X2                              101110
    X3                              50
    X4                              110
    X5                              101110
    MSG                             point-17
    N1                              <null>
    N2                              60
    X1                              110
    X2                              101060
    X3                              60
    X4                              110
    X5                              101060
    MSG                             point-17
    N1                              1
    N2                              100
    X1                              111
    X2                              10100
    X3                              100
    X4                              100
    X5                              10100
    MSG                             point-17
    N1                              3
    N2                              300
    X1                              300
    X2                              300
    X3                              300
    X4                              300
    X5                              300
    MSG                             point-17
    N1                              5
    N2                              500
    X1                              500
    X2                              500
    X3                              500
    X4                              500
    X5                              500
    MSG                             point-17
    N1                              <null>
    N2                              1000
    X1                              1110
    X2                              101000
    X3                              1000
    X4                              1000
    X5                              101000
    MSG                             point-17
    N1                              1
    N2                              10000
    X1                              10111
    X2                              10000
    X3                              10000
    X4                              10000
    X5                              10000
    MSG                             point-17
    N1                              <null>
    N2                              100000
    X1                              101110
    X2                              100000
    X3                              100000
    X4                              100000
    X5                              100000
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    sql_init = (act.files_dir / 'gtcs-window-func.sql').read_text()

    sql_addi= \
    """
        set list on;

        recreate table t1 (
          n1 integer,
          n2 integer
        );
        commit;

        insert into t1 values (null, 100000);
        insert into t1 values (null, 1000);
        insert into t1 values (1, 1);
        insert into t1 values (1, 10);
        insert into t1 values (1, 100);
        insert into t1 values (1, 10000);
        insert into t1 values (2, 20);
        insert into t1 values (3, 300);
        insert into t1 values (5, 500);
        insert into t1 values (null, 50);
        insert into t1 values (null, 60);
        commit;

        select 
               'point-01' as msg,
               n1,
               n2,
               sum(n2) over (partition by n1 order by n2 range between unbounded preceding and current row) x1,
               sum(n2) over (partition by n1 order by n2 range between current row and unbounded following) x2,
               sum(n2) over (partition by n1 order by n2 range between current row and current row) x3,
               sum(n2) over (partition by n1 order by n2 range between 2 following and 3 following) x4,
               sum(n2) over (partition by n1 order by n2 range between 3 preceding and 2 preceding) x5,
               sum(n2) over (partition by n1 order by n2 range between 3 preceding and unbounded following) x6
        from t1
        order by n2, n1;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-02' as msg,
               n1,
               n2,
               sum(n2) over (partition by n1 order by n2, n1 rows between unbounded preceding and current row) x1,
               sum(n2) over (partition by n1 order by n2, n1 rows between current row and unbounded following) x2,
               sum(n2) over (partition by n1 order by n2, n1 rows between current row and current row) x3,
               sum(n2) over (partition by n1 order by n2, n1 rows between 2 following and 3 following) x4,
               sum(n2) over (partition by n1 order by n2, n1 rows between 3 preceding and 2 preceding) x5,
               sum(n2) over (partition by n1 order by n2, n1 rows between 3 preceding and unbounded following) x6
        from t1
        order by n2, n1;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-03' as msg,
               n1,
               n2,
               sum(n2) over (order by n1 range between unbounded preceding and current row) x1,
               sum(n2) over (order by n1 range between current row and unbounded following) x2,
               sum(n2) over (order by n1 range between current row and current row) x3,
               sum(n2) over (order by n1 range between 2 following and 3 following) x4,
               sum(n2) over (order by n1 range between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1 range between 3 preceding and unbounded following) x6
        from t1
        order by n1, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-04' as msg,
               n1,
               n2,
               sum(n2) over (order by n1, n2 rows between unbounded preceding and current row) x1,
               sum(n2) over (order by n1, n2 rows between current row and unbounded following) x2,
               sum(n2) over (order by n1, n2 rows between current row and current row) x3,
               sum(n2) over (order by n1, n2 rows between 2 following and 3 following) x4,
               sum(n2) over (order by n1, n2 rows between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1, n2 rows between 3 preceding and unbounded following) x6
        from t1
        order by n1, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-05' as msg,
               n1,
               n2,
               sum(n2) over (order by n1 nulls first range between unbounded preceding and current row) x1,
               sum(n2) over (order by n1 nulls first range between current row and unbounded following) x2,
               sum(n2) over (order by n1 nulls first range between current row and current row) x3,
               sum(n2) over (order by n1 nulls first range between 2 following and 3 following) x4,
               sum(n2) over (order by n1 nulls first range between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1 nulls first range between 3 preceding and unbounded following) x6
          from t1
          order by n1 nulls first, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-06' as msg,
               n1,
               n2,
               sum(n2) over (order by n1 nulls first, n2 rows between unbounded preceding and current row) x1,
               sum(n2) over (order by n1 nulls first, n2 rows between current row and unbounded following) x2,
               sum(n2) over (order by n1 nulls first, n2 rows between current row and current row) x3,
               sum(n2) over (order by n1 nulls first, n2 rows between 2 following and 3 following) x4,
               sum(n2) over (order by n1 nulls first, n2 rows between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1 nulls first, n2 rows between 3 preceding and unbounded following) x6
          from t1
          order by n1 nulls first, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-07' as msg,
               n1,
               n2,
               sum(n2) over (order by n1 nulls last range between unbounded preceding and current row) x1,
               sum(n2) over (order by n1 nulls last range between current row and unbounded following) x2,
               sum(n2) over (order by n1 nulls last range between current row and current row) x3,
               sum(n2) over (order by n1 nulls last range between 2 following and 3 following) x4,
               sum(n2) over (order by n1 nulls last range between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1 nulls last range between 3 preceding and unbounded following) x6
          from t1
          order by n1 nulls last, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-08' as msg,
               n1,
               n2,
               sum(n2) over (order by n1 nulls last, n2 rows between unbounded preceding and current row) x1,
               sum(n2) over (order by n1 nulls last, n2 rows between current row and unbounded following) x2,
               sum(n2) over (order by n1 nulls last, n2 rows between current row and current row) x3,
               sum(n2) over (order by n1 nulls last, n2 rows between 2 following and 3 following) x4,
               sum(n2) over (order by n1 nulls last, n2 rows between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1 nulls last, n2 rows between 3 preceding and unbounded following) x6
        from t1
        order by n1 nulls last, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-09' as msg,
               n1,
               n2,
               sum(n2) over (order by n1 desc range between unbounded preceding and current row) x1,
               sum(n2) over (order by n1 desc range between current row and unbounded following) x2,
               sum(n2) over (order by n1 desc range between current row and current row) x3,
               sum(n2) over (order by n1 desc range between 2 following and 3 following) x4,
               sum(n2) over (order by n1 desc range between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1 desc range between 3 preceding and unbounded following) x6
        from t1
        order by n1 desc, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select 
               'point-10' as msg,
               n1,
               n2,
               sum(n2) over (order by n1 desc, n2 rows between unbounded preceding and current row) x1,
               sum(n2) over (order by n1 desc, n2 rows between current row and unbounded following) x2,
               sum(n2) over (order by n1 desc, n2 rows between current row and current row) x3,
               sum(n2) over (order by n1 desc, n2 rows between 2 following and 3 following) x4,
               sum(n2) over (order by n1 desc, n2 rows between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1 desc, n2 rows between 3 preceding and unbounded following) x6
        from t1
        order by n1 desc, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-11' as msg,
               n1,
               n2,
               sum(n2) over (order by n1 desc nulls first range between unbounded preceding and current row) x1,
               sum(n2) over (order by n1 desc nulls first range between current row and unbounded following) x2,
               sum(n2) over (order by n1 desc nulls first range between current row and current row) x3,
               sum(n2) over (order by n1 desc nulls first range between 2 following and 3 following) x4,
               sum(n2) over (order by n1 desc nulls first range between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1 desc nulls first range between 3 preceding and unbounded following) x6
        from t1
        order by n1 desc nulls first, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-12' as msg,
               n1,
               n2,
               sum(n2) over (order by n1 desc nulls first, n2 rows between unbounded preceding and current row) x1,
               sum(n2) over (order by n1 desc nulls first, n2 rows between current row and unbounded following) x2,
               sum(n2) over (order by n1 desc nulls first, n2 rows between current row and current row) x3,
               sum(n2) over (order by n1 desc nulls first, n2 rows between 2 following and 3 following) x4,
               sum(n2) over (order by n1 desc nulls first, n2 rows between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1 desc nulls first, n2 rows between 3 preceding and unbounded following) x6
        from t1
        order by n1 desc nulls first, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-13' as msg,
               n1,
               n2,
               sum(n2) over (order by n1 desc nulls last range between unbounded preceding and current row) x1,
               sum(n2) over (order by n1 desc nulls last range between current row and unbounded following) x2,
               sum(n2) over (order by n1 desc nulls last range between current row and current row) x3,
               sum(n2) over (order by n1 desc nulls last range between 2 following and 3 following) x4,
               sum(n2) over (order by n1 desc nulls last range between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1 desc nulls last range between 3 preceding and unbounded following) x6
        from t1
        order by n1 desc nulls last, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-14' as msg,
               n1,
               n2,
               sum(n2) over (order by n1 desc nulls last, n2 rows between unbounded preceding and current row) x1,
               sum(n2) over (order by n1 desc nulls last, n2 rows between current row and unbounded following) x2,
               sum(n2) over (order by n1 desc nulls last, n2 rows between current row and current row) x3,
               sum(n2) over (order by n1 desc nulls last, n2 rows between 2 following and 3 following) x4,
               sum(n2) over (order by n1 desc nulls last, n2 rows between 3 preceding and 2 preceding) x5,
               sum(n2) over (order by n1 desc nulls last, n2 rows between 3 preceding and unbounded following) x6
        from t1
        order by n1 desc nulls last, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-15' as msg,
               n1,
               n2,
               count(*) over (order by n1 nulls first range between coalesce(n1, 0) preceding and coalesce(n1, 0) preceding) x1,
               sum(n2) over (order by n1 nulls first range between coalesce(n1, 0) preceding and coalesce(n1, 0) preceding) x2,
               count(*) over (order by n1 nulls first range between coalesce(n1, 0) following and coalesce(n1, 0) following) x3,
               sum(n2) over (order by n1 nulls first range between coalesce(n1, 0) following and coalesce(n1, 0) following) x4
        from t1
        order by n1 nulls first, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-16' as msg,
               n1,
               n2,
               count(*) over (order by n1 nulls first, n2 rows between coalesce(n1, 0) preceding and coalesce(n1, 0) preceding) x1,
               sum(n2) over (order by n1 nulls first, n2 rows between coalesce(n1, 0) preceding and coalesce(n1, 0) preceding) x2,
               count(*) over (order by n1 nulls first, n2 rows between coalesce(n1, 0) following and coalesce(n1, 0) following) x3,
               sum(n2) over (order by n1 nulls first, n2 rows between coalesce(n1, 0) following and coalesce(n1, 0) following) x4
        from t1
        order by n1 nulls first, n2;

        --+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

        select  
               'point-17' as msg,
               n1,
               n2,
               sum(n2) over (partition by n1 order by date '2000-01-01' + n2 range between unbounded preceding and current row) x1,
               sum(n2) over (partition by n1 order by date '2000-01-01' + n2 range between current row and unbounded following) x2,
               sum(n2) over (partition by n1 order by date '2000-01-01' + n2 range between current row and current row) x3,
               sum(n2) over (partition by n1 order by date '2000-01-01' + n2 range between 10 preceding and 10 following) x4,
               sum(n2) over (partition by n1 order by date '2000-01-01' + n2 range between 3 preceding and unbounded following) x5
        from t1
        order by n2, n1;
    """

    act.expected_stdout = test_expected_stdout

    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ) )

    assert act.clean_stdout == act.clean_expected_stdout
