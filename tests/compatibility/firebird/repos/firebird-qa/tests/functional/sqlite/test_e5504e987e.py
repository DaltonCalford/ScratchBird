#coding:utf-8

"""
ID:          e5504e987e
ISSUE:       https://www.sqlite.org/src/tktview/e5504e987e
TITLE:       Segfault when running query that uses NTILE()OVER()
DESCRIPTION:
    Source code contains epression as argument to NTILE() function.
    This currently not allowed in FB, so it was decided just to check ES with passing arguments of misc datatypes and values.
    All input arguments can not be applied and must case exceptions.
NOTES:
    [14.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns(n bigint) as begin execute statement ('select ntile(?)over(order by 1) from rdb$database')(null) into n; end ^
    execute block as declare n bigint; begin execute statement ('select ntile(?)over(order by 1) from rdb$database')(0) into n; end ^
    execute block returns(n bigint) as begin execute statement ('select ntile(?)over(order by 1) from rdb$database')('QWE') into n; end ^
    execute block returns(n bigint) as begin execute statement ('select ntile(?)over(order by 1) from rdb$database')(true) into n; end ^
    execute block returns(n bigint) as begin execute statement ('select ntile(?)over(order by 1) from rdb$database')(current_time) into n; end ^
    execute block as declare n bigint; begin execute statement ('select ntile(?)over(order by 1) from rdb$database')(9223372036854775808) into n; end ^
    execute block returns(n bigint) as begin execute statement ('select ntile(?)over(order by 1) from rdb$database')(cast(9.999999999999999999999999999999999E6144 as decfloat(34))) into n; end ^
    execute block returns(n bigint) as begin execute statement ('select ntile(?)over(order by 1) from rdb$database')(cast(1.0E-6143 as decfloat(34))) into n; end ^
    set term ;^
"""

substitutions = [('[ \t]+', ' '), ('-At block.*', ''), ('conversion error from string ".*', 'conversion error from string')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 42000
    Argument #1 for NTILE must be positive

    Statement failed, SQLSTATE = 42000
    Argument #1 for NTILE must be positive

    Statement failed, SQLSTATE = 22018
    conversion error from string "QWE"

    Statement failed, SQLSTATE = 22018
    conversion error from string "BOOLEAN"

    Statement failed, SQLSTATE = 22018
    conversion error from string "23:00:50.0000 Europe/Moscow"

    Statement failed, SQLSTATE = 22003
    arithmetic exception, numeric overflow, or string truncation
    -Integer overflow. The result of an integer operation caused the most significant bit of the result to carry.

    Statement failed, SQLSTATE = 22000
    Decimal float invalid operation. An indeterminant error occurred during an operation.

    Statement failed, SQLSTATE = 42000
    Argument #1 for NTILE must be positive
"""

@pytest.mark.version('>=4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
