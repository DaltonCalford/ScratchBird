#coding:utf-8

"""
ID:          issue-6135
ISSUE:       6135
TITLE:       Provide name of udf function for "arithmetic exception, numeric overflow, or string truncation"
DESCRIPTION:
  *** FOR FB 3.X ONLY ***
  Test uses UDF 'sright' declared in ib_udf.sql script which for sure present in every FB snapshot.
  After this, we try to create PSQL function with the same signature.

  *** FOR FB 4.X AND ABOVE  ***
  Added separate code for running on FB 4.0.x: use udf_compat!UC_div from UDR engine.
  It seems that this UDR function is the only wich we can force to raise exception by passing invalid
  argument #2 that will cause zero divide.

  UDF usage is deprecated in FB 4+, see: ".../doc/README.incompatibilities.3to4.txt".
  Functions div, frac, dow, sdow, getExactTimestampUTC and isLeapYear got safe replacement
  in UDR library "udf_compat", see it in folder: ../plugins/udr/
JIRA:        CORE-5876
FBTEST:      bugs.core_5876
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

# version: 3.0

test_script_1 = """
    set term ^;
    execute block as
    begin
        execute statement 'drop function substr';
        when any do begin end
    end
    ^
    set term ;^
    commit;

    declare external function substr
    cstring(80), smallint, smallint
    returns cstring(80)
    entry_point 'IB_UDF_substr'
    module_name 'ib_udf'
    ;
    commit;

    set heading off;
    select substr(cast('abc' as char(1500)) || '123', 1, 1000)
    from rdb$database
    ;
"""

act_1 = isql_act('db', test_script_1)

expected_stderr_1 = """
    Statement failed, SQLSTATE = 22001
    arithmetic exception, numeric overflow, or string truncation
    -string right truncation
    -expected length 80, actual 1503
    -UDF: SUBSTR
"""

@pytest.mark.version('>=3.0.4,<4.0')
def test_1(act_1: Action):
    act_1.expected_stderr = expected_stderr_1
    act_1.execute()
    assert act_1.clean_stderr == act_1.clean_expected_stderr

# version: 4.0

test_script_2 = """
    create function UDR40_div (
        n1 integer,
        n2 integer
    ) returns double precision
        external name 'udf_compat!UC_div'
        engine udr;


    commit;
    set list on;
    select UDR40_div( 1, 0) from rdb$database;
"""

act_2 = isql_act('db', test_script_2)

expected_stderr_5x = """
    Statement failed, SQLSTATE = 22012
    arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
    -At function 'UDR40_DIV'
"""

expected_stderr_6x = """
    Statement failed, SQLSTATE = 22012
    arithmetic exception, numeric overflow, or string truncation
    -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
    -At function "PUBLIC"."UDR40_DIV"
"""


@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stderr = expected_stderr_5x if act_2.is_version('<6') else expected_stderr_6x
    act_2.execute()
    assert act_2.clean_stderr == act_2.clean_expected_stderr
