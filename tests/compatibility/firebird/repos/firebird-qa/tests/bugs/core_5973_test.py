#coding:utf-8

"""
ID:          issue-6225
ISSUE:       6225
TITLE:       Handling FP overflow in double precision value when converting from decfloat
DESCRIPTION:
NOTES:
[05.03.2021]
  added subst.: max. floating point precision on Linux is 15 rather than on Windows (16 digits).
JIRA:        CORE-5973
NOTES:
    [24.05.2025] pzotov
    Splitted expected* variables for versions up to 5.x and 6.x+
    This is needed after 11d5d5 ("Fix for #8082 ... user buffers directly (#8145)") by Dmitry Sibiryakov.
    Discussed in email 24.05.2025 22:06, subj: "one more consequence of 11d5d5 ..." (since 15.05.2025 17:25).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    -- OLD SYNTAX: set decfloat bind double precision;
    -- Syntax after 11-nov-2019:
    -- https://github.com/FirebirdSQL/firebird/commit/a77295ba153e0c17061e2230d0ffdbaf08839114
    -- See also: doc/sql.extensions/README.set_bind.md:
    --     SET BIND OF type-from TO { type-to | LEGACY };
    --     SET BIND OF type NATIVE;

    set bind of decfloat to double precision;

    set decfloat traps to Overflow, Underflow;

    -- Following four statements should raise exception:
    -- #################################################


    -- Statement failed, SQLSTATE = 22003
    -- Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.
    select cast( 9.999999999999999999999999999999999E6144 as decfloat(34)) as greatest_df34_for_pos_scope from rdb$database;
    select cast(-9.999999999999999999999999999999999E6144 as decfloat(34)) as freatest_df34_for_neg_scope from rdb$database;

    -- Statement failed, SQLSTATE = 22003
    -- Decimal float underflow.  The exponent of a result is less than the magnitude allowed.
    select cast(1.0E-6143 as decfloat(34)) as approx_zero_df34_for_pos_scope from rdb$database;
    select cast(-1.0E-6143 as decfloat(34)) as approx_zero__df34_for_neg_scope from rdb$database;


    set decfloat traps to Inexact;

    -- Following four statements should NOT raise exception and issue: Infinity, Infinity, 0.000..., 0.000...:
    -- ##############################################################

    --  This must issue Infinity instead of 0.000...:
    select cast( 9.999999999999999999999999999999999E6144 as decfloat(34)) as greatest_df34_for_pos_scope from rdb$database;

    --  This must issue -Infinity instead of 0.000...:
    select cast(-9.999999999999999999999999999999999E6144 as decfloat(34)) as greatest_df34_for_neg_scope from rdb$database;

    select cast(1.0E-6143 as decfloat(34)) as approx_zero_df34_for_pos_scope from rdb$database;

    select cast(-1.0E-6143 as decfloat(34)) as approx_zero_df34_for_neg_scope from rdb$database;

    quit;

"""

act = isql_act('db', test_script, substitutions=[('0.0000000000000000', '0.000000000000000')])

expected_out_5x = """
    Statement failed, SQLSTATE = 22003
    Dynamic SQL Error
    -SQL error code = -303
    -Floating-point overflow.  The exponent of a floating-point operation is greater than the magnitude allowed.

    Statement failed, SQLSTATE = 22003
    Dynamic SQL Error
    -SQL error code = -303
    -Floating-point overflow.  The exponent of a floating-point operation is greater than the magnitude allowed.

    Statement failed, SQLSTATE = 22003
    Dynamic SQL Error
    -SQL error code = -303
    -Floating-point underflow.  The exponent of a floating-point operation is less than the magnitude allowed.

    Statement failed, SQLSTATE = 22003
    Dynamic SQL Error
    -SQL error code = -303
    -Floating-point underflow.  The exponent of a floating-point operation is less than the magnitude allowed.

    GREATEST_DF34_FOR_POS_SCOPE     Infinity
    GREATEST_DF34_FOR_NEG_SCOPE     -Infinity
    APPROX_ZERO_DF34_FOR_POS_SCOPE  0.000000000000000
    APPROX_ZERO_DF34_FOR_NEG_SCOPE  0.000000000000000
"""

expected_out_6x = """
    Statement failed, SQLSTATE = 22003
    Floating-point overflow.  The exponent of a floating-point operation is greater than the magnitude allowed.

    Statement failed, SQLSTATE = 22003
    Floating-point overflow.  The exponent of a floating-point operation is greater than the magnitude allowed.

    Statement failed, SQLSTATE = 22003
    Floating-point underflow.  The exponent of a floating-point operation is less than the magnitude allowed.

    Statement failed, SQLSTATE = 22003
    Floating-point underflow.  The exponent of a floating-point operation is less than the magnitude allowed.

    GREATEST_DF34_FOR_POS_SCOPE     Infinity
    GREATEST_DF34_FOR_NEG_SCOPE     -Infinity
    APPROX_ZERO_DF34_FOR_POS_SCOPE  0.000000000000000
    APPROX_ZERO_DF34_FOR_NEG_SCOPE  0.000000000000000
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_out_5x if act.is_version('<6') else expected_out_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
