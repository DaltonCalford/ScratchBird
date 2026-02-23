#coding:utf-8

"""
ID:          afdc5a29dc
ISSUE:       https://www.sqlite.org/src/tktview/afdc5a29dc
TITLE:       Lossless conversion when casting a large TEXT number to NUMERIC is not performed
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Test checks ability to get exact values when they are specified as text and represent scientific notation
    for appropriate precision bounds.
    On 3.x numeric overflow raises with different SQLSTATE. Test not checks this version.
    NB: we test *only* dialect 3.
    See: https://firebirdsql.org/file/documentation/chunk/en/refdocs/fblangref50/fblangref50-datatypes-fixedtypes.html#fblangref50-datatypes-numeric
    Table 3.5. Method of Physical Storage for Fixed-Point Numbers
   
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    -- For precision 1...4 values of NUMERIC type are stored as SMALLINT,
    -- scope: [-32768 ... 32767]:
    select cast('-3.2768e4' as numeric(1)) as min_for_numeric_1 from rdb$database; -- expected: -32768
    select cast('3.2767e4' as numeric(1)) as max_for_numeric_1 from rdb$database;  -- expected: 32767
    select cast('-3.2769e4' as numeric(1)) from rdb$database; -- expected: num ovf
    select cast('3.2768e4' as numeric(1)) from rdb$database;  -- expected: num ovf
    
    -- For precision 5...9 values of NUMERIC type are stored as INT,
    -- scope: [-2147483648 ... 2147483647]:
    select cast('-2.147483648e9' as numeric(9)) as min_for_numeric_9 from rdb$database; -- expected: -2147483648
    select cast('2.147483647e9' as numeric(9)) as min_for_numeric_9 from rdb$database;  -- expected: 2147483647

    select cast('-2.147483649e9' as numeric(9)) from rdb$database; -- expected: num ovf
    select cast('2.147483648e9' as numeric(9)) from rdb$database;  -- expected: num ovf

    -- For precision 10...18 values of NUMERIC type are stored as BIGINT,
    -- scope: [-9223372036854775808 ... 9223372036854775807]:
    select cast('-9.223372036854775808e18' as numeric(10)) as min_for_numeric_10 from rdb$database; -- expected: -9223372036854775808
    select cast('9.223372036854775807e18' as numeric(10)) as max_for_numeric_10 from rdb$database;  -- expected: 9223372036854775807
    select cast('-9.223372036854775809e18' as numeric(10)) from rdb$database; -- expected: num ovf
    select cast('9.223372036854775808e18' as numeric(10)) from rdb$database;  -- expected: num ovf

    -- For precision 19...38 values of NUMERIC type are stored as INT128,
    -- scope: [-170141183460469231731687303715884105728 ... 170141183460469231731687303715884105727]:
    select cast('-1.70141183460469231731687303715884105728e38' as numeric(19)) as min_for_numeric_19 from rdb$database; -- -170141183460469231731687303715884105728
    select cast('1.70141183460469231731687303715884105727e38' as numeric(19)) as max_for_numeric_19 from rdb$database;  -- 170141183460469231731687303715884105727
    select cast('-1.70141183460469231731687303715884105729e38' as numeric(19)) from rdb$database; -- expected: num ovf
    select cast('1.70141183460469231731687303715884105728e38' as numeric(19)) from rdb$database;  -- expected: num ovf
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    MIN_FOR_NUMERIC_1 -32768
    MAX_FOR_NUMERIC_1 32767
    
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range

    MIN_FOR_NUMERIC_9 -2147483648
    MIN_FOR_NUMERIC_9 2147483647
    
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    
    MIN_FOR_NUMERIC_10 -9223372036854775808
    MAX_FOR_NUMERIC_10 9223372036854775807
    
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    
    MIN_FOR_NUMERIC_19 -170141183460469231731687303715884105728
    MAX_FOR_NUMERIC_19 170141183460469231731687303715884105727
    
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
    
    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -numeric value is out of range
"""

@pytest.mark.version('>=4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
