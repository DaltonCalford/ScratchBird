#coding:utf-8

"""
ID:          decfloat.nan-and-infinity-comparison
TITLE:       DECFLOAT should not throw exceptions when +/-NaN, +/-sNaN and +/-Infinity is used in comparisons
DESCRIPTION:
FBTEST:      functional.datatypes.decfloat_nan_and_infinity_comparison
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- NB: need to set decfloat traps to <EMPTY>, otherwise get:
    -- Statement failed, SQLSTATE = 22012
    -- Decimal float divide by zero.  The code attempted to divide a DECFLOAT value by zero.
    set decfloat traps to;
    set list on;
    set count on;
    select
        i
       ,n
       ,i < i+i as is_infinity_less_then_itself_plus_same_infinity
       ,i = i-i is_infinity_equal_to_itself_reduced_by_any_value
       ,n < n + i is_nan_less_than_nan_plus_infinity
       ,n = n - 1 is_nan_equal_to_nan_minus_a_number
       ,n = n+1 is_nan_equal_to_nan_plus_a_number
       ,n > n - i is_nan_more_than_nan_minus_infinity
    from (
        select 1/1e-9999 as i, cast('34ffd' as decfloat(16)) as n from rdb$database
    );
"""

act = isql_act('db', test_script, substitutions=[('[\\s]+', ' ')])

expected_stdout = """
    I Infinity
    N NaN
    IS_INFINITY_LESS_THEN_ITSELF_PLUS_SAME_INFINITY <false>
    IS_INFINITY_EQUAL_TO_ITSELF_REDUCED_BY_ANY_VALUE <true>
    IS_NAN_LESS_THAN_NAN_PLUS_INFINITY <false>
    IS_NAN_EQUAL_TO_NAN_MINUS_A_NUMBER <true>
    IS_NAN_EQUAL_TO_NAN_PLUS_A_NUMBER <true>
    IS_NAN_MORE_THAN_NAN_MINUS_INFINITY <false>

    Records affected: 1
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
