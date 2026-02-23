#coding:utf-8

"""
ID:          decfloat.exceptions-trapping
ISSUE:       5803
JIRA:        CORE-5535
TITLE:       Test exception trapping for result of DECFLOAT operations
DESCRIPTION:
  See  doc/sql.extensions/README.data_types

  SET DECFLOAT TRAPS TO <comma-separated traps list - may be empty> - controls which
  exceptional conditions cause a trap. Valid traps are: Division_by_zero, Inexact,
  Invalid_operation, Overflow and Underflow. By default traps are set to:
  Division_by_zero, Invalid_operation, Overflow, Underflow.
FBTEST:      functional.datatypes.decfloat_exceptions_trapping
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    ------------------------- check: empty vs Division_by_zero ------------------------
    set decfloat traps to;
    -- Should issue: Infinity
    select 1/1e-9999 zero_div_when_df_trap_empty
    from rdb$database;


    set decfloat traps to Division_by_zero;
    -- Statement failed, SQLSTATE = 22012
    -- Decimal float divide by zero.  The code attempted to divide a DECFLOAT value by zero.
    select 1/1e-9999 zero_div_when_df_trap_zd
    from rdb$database;

    ------------------------- check: empty vs Overflow ------------------------
    set decfloat traps to;
    -- Should issue: Infinity
    select 1e9999 huge_when_df_trap_empty
    from rdb$database;

    set decfloat traps to Overflow;
    -- Statement failed, SQLSTATE = 22003
    -- Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.
    select 1e9999 huge_when_df_trap_overflow
    from rdb$database;

    ------------------------- check: empty vs Underflow ------------------------

    set decfloat traps to;
    -- Issues: 0E-6176
    select 1e-9999 about_zero_when_df_trap_empty
    from rdb$database;

    set decfloat traps to Underflow;
    -- Statement failed, SQLSTATE = 22003
    -- Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.
    select 1e-9999 about_zero_when_df_trap_overflow
    from rdb$database;



    ------------------------- check: empty vs Inexact ------------------------

    set decfloat traps to;

    -- Should issue: Infinity
    select 1e9999 + 1e9999 as add_huges_when_df_trap_empty
    from rdb$database;

    set decfloat traps to Inexact;

    -- Statement failed, SQLSTATE = 22000
    -- Decimal float inexact result.  The result of an operation cannot be represented as a decimal fraction.
    select 1e9999 + 1e9999 add_huges_when_df_trap_inexact
    from rdb$database;


    ------------------------- check: empty vs Invalid_operation ------------------------

    -- Sample by Alex, letter 25.05.2017 20:30

    set decfloat traps to;
    -- Should issue: NaN
    select cast('34ffd' as decfloat(16)) nan_when_df_trap_empty
    from rdb$database;

    set decfloat traps to Invalid_operation;
    -- Statement failed, SQLSTATE = 22000
    -- Decimal float invalid operation.  An indeterminant error occurred during an operation.
    select cast('34ffd' as decfloat(16)) nan_when_df_trap_inv_op
    from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    ZERO_DIV_WHEN_DF_TRAP_EMPTY                                       Infinity
    HUGE_WHEN_DF_TRAP_EMPTY                                           Infinity
    ABOUT_ZERO_WHEN_DF_TRAP_EMPTY                                      0E-6176
    ADD_HUGES_WHEN_DF_TRAP_EMPTY                                      Infinity
    NAN_WHEN_DF_TRAP_EMPTY                              NaN

"""

expected_stderr = """
    Statement failed, SQLSTATE = 22012
    Decimal float divide by zero.  The code attempted to divide a DECFLOAT value by zero.

    Statement failed, SQLSTATE = 22003
    Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.

    Statement failed, SQLSTATE = 22003
    Decimal float underflow.  The exponent of a result is less than the magnitude allowed.

    Statement failed, SQLSTATE = 22000
    Decimal float inexact result.  The result of an operation cannot be represented as a decimal fraction.

    Statement failed, SQLSTATE = 22018
    Decimal float invalid operation.  An indeterminant error occurred during an operation.
    -conversion error from string "34ffd"
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
