#coding:utf-8

"""
ID:          issue-2992
ISSUE:       2992
TITLE:       Infinity from UDF should be trapped by the engine
DESCRIPTION:
NOTES:
[24.01.2019]
  Disabled this test to be run on FB 4.0
  UDF usage is deprecated in FB 4+, see: ".../doc/README.incompatibilities.3to4.txt".
JIRA:        CORE-2582
FBTEST:      bugs.core_2582
"""

import pytest
from firebird.qa import *

init_script = """
    DECLARE EXTERNAL FUNCTION DPOWER
    DOUBLE PRECISION BY DESCRIPTOR, DOUBLE PRECISION BY DESCRIPTOR, DOUBLE PRECISION BY DESCRIPTOR
    RETURNS PARAMETER 3
    ENTRY_POINT 'power' MODULE_NAME 'fbudf';

    DECLARE EXTERNAL FUNCTION XASIN
    DOUBLE PRECISION
    RETURNS DOUBLE PRECISION BY VALUE
    ENTRY_POINT 'IB_UDF_asin' MODULE_NAME 'ib_udf';

    DECLARE EXTERNAL FUNCTION XDIV
    INTEGER, INTEGER
    RETURNS DOUBLE PRECISION BY VALUE
    ENTRY_POINT 'IB_UDF_div' MODULE_NAME 'ib_udf';
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select dpower(1e120, 3) from rdb$database;
    select xasin(2) from rdb$database;
    select xdiv(10, 0) from rdb$database;
"""

act = isql_act('db', test_script)

expected_stderr = """
    Statement failed, SQLSTATE = 39000
    expression evaluation not supported
    -Floating point overflow in result from UDF DPOWER
    -UDF: DPOWER

    Statement failed, SQLSTATE = 39000
    expression evaluation not supported
    -Invalid floating point value returned by UDF XASIN
    -UDF: XASIN

    Statement failed, SQLSTATE = 39000
    expression evaluation not supported
    -Floating point overflow in result from UDF XDIV
    -UDF: XDIV
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

