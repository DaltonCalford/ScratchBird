#coding:utf-8

"""
ID:          issue-1597
ISSUE:       1597
TITLE:       Error "Data type unknown" when any UDF argument is a built-in function containing a DSQL parameter reference
DESCRIPTION:
  For FB 3.x - this test uses UDF from ib_udf.
  UDF usage is deprecated in FB 4+, see: ".../doc/README.incompatibilities.3to4.txt".
  Functions div, frac, dow, sdow, getExactTimestampUTC and isLeapYear got safe replacement
  in UDR library "udf_compat", see it in folder: ../plugins/udr/
JIRA:        CORE-1175
FBTEST:      bugs.core_1175
"""
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

# version: 3.0

init_script_1 = """DECLARE EXTERNAL FUNCTION rtrim
   CSTRING(255)
   RETURNS CSTRING(255) FREE_IT
   ENTRY_POINT 'IB_UDF_rtrim' MODULE_NAME 'ib_udf';
commit;
"""

db_1 = db_factory(init=init_script_1)

act_1 = python_act('db_1')

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    with  act_1.db.connect() as con:
        cur = con.cursor()
        ps = None
        try:
            ps = cur.prepare('select * from RDB$DATABASE where RDB$CHARACTER_SET_NAME = rtrim(trim(?))')
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()

# version: 4.0

init_script_2 = """
    -- See declaration sample in plugins\\udr\\UdfBackwardCompatibility.sql:
    create function UDR40_frac (
        val double precision
    ) returns double precision
    external name 'udf_compat!UC_frac'
    engine udr;

    create function UDR40_div (
        n1 integer,
        n2 integer
    ) returns double precision
    external name 'udf_compat!UC_div'
    engine udr;

    commit;
  """

db_2 = db_factory(init=init_script_2)

act_2 = python_act('db_2')

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    with  act_2.db.connect() as con:
        cur = con.cursor()
        ps = None
        try:
            ps = cur.prepare('select 1 from rdb$database where UDR40_frac(?) != UDR40_div(?, ?) / ?')
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if ps:
                ps.free()
