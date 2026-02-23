#coding:utf-8

"""
ID:          issue-1930
ISSUE:       1930
TITLE:       Many new 2.1 built in functions have incorrect NULL semantics
DESCRIPTION:
JIRA:        CORE-1514
FBTEST:      bugs.core_1514
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """SELECT ABS(null) from rdb$database;
SELECT ACOS(null) from rdb$database;
SELECT ASCII_VAL(null) from rdb$database;
SELECT ASIN(null) from rdb$database;
SELECT ATAN(null) from rdb$database;
SELECT ATAN2(null,null) from rdb$database;
SELECT BIN_AND(null,null) from rdb$database;
SELECT BIN_OR(null,null) from rdb$database;
SELECT BIN_XOR(null,null) from rdb$database;
SELECT COS(null) from rdb$database;
SELECT COSH(null) from rdb$database;
SELECT COT(null) from rdb$database;
SELECT dateadd(year, null, current_date) from rdb$database;
SELECT FLOOR(null) from rdb$database;
SELECT LN(null) from rdb$database;
SELECT LOG(null,null) from rdb$database;
SELECT LOG10(null) from rdb$database;
SELECT MOD(null,null) from rdb$database;
SELECT SIGN(null) from rdb$database;
SELECT SIN(null) from rdb$database;
SELECT SINH(null) from rdb$database;
SELECT SQRT(null) from rdb$database;
SELECT TAN(null) from rdb$database;
SELECT TANH(null) from rdb$database;
select trunc(1, cast(null as integer)) from rdb$database;"""

act = isql_act('db', test_script)

expected_stdout = """
         ABS
============
      <null>


                   ACOS
=======================
                 <null>


ASCII_VAL
=========
   <null>


                   ASIN
=======================
                 <null>


                   ATAN
=======================
                 <null>


                  ATAN2
=======================
                 <null>


     BIN_AND
============
      <null>


      BIN_OR
============
      <null>


     BIN_XOR
============
      <null>


                    COS
=======================
                 <null>


                   COSH
=======================
                 <null>


                    COT
=======================
                 <null>


    DATEADD
===========
     <null>


       FLOOR
============
      <null>


                     LN
=======================
                 <null>


                    LOG
=======================
                 <null>


                  LOG10
=======================
                 <null>


         MOD
============
      <null>


   SIGN
=======
 <null>


                    SIN
=======================
                 <null>


                   SINH
=======================
                 <null>


                   SQRT
=======================
                 <null>


                    TAN
=======================
                 <null>


                   TANH
=======================
                 <null>


       TRUNC
============
      <null>

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

