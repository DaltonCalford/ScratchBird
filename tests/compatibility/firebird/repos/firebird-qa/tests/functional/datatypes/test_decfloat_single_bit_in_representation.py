#coding:utf-8

"""
ID:          decfloat.single-bit-in-representation
TITLE:       Check result of EXP() which can be represented only by one ("last") significant bit
DESCRIPTION:
  Get minimal distinguish from zero value for DEFCFLOAT datatype using EXP() function.
  Check some trivial arithmetic results for this value and pair of other values which are closest to it.
  See also: https://en.wikipedia.org/wiki/Decimal_floating_point
FBTEST:      functional.datatypes.decfloat_single_bit_in_representation
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
         df0 as decfloat_near_zero
        ,df1 as min_decfloat_distinguish_from_zero
        ,df2 as max_decfloat_non_distinct_from_zero
        ,df0/df1 as division_result_1a
        ,df1/df0 as division_result_1b
        ,df0-df1 as subtract_result_1a
        ,df1-df0 as subtract_result_1b
        ,df0+df2 = df1 as sum_cmp_result_2
        ,df1 / df1 division_result_2
        ,df1+df2 = df1 sum_and_cmp_result_2
    from (
        select
            exp( cast( -14221.4586815117860898045324562520948 as decfloat) ) as df0
           ,exp( cast( -14221.4586815117860898045324562520949 as decfloat) ) as df1
           ,exp( cast( -14221.4586815117860898045324562520950 as decfloat) ) as df2
        from rdb$database
    );
"""

act = isql_act('db', test_script, substitutions=[('[ ]+', ' ')])

expected_stdout = """
    DECFLOAT_NEAR_ZERO                                                 1E-6176
    MIN_DECFLOAT_DISTINGUISH_FROM_ZERO                                    1E-6176
    MAX_DECFLOAT_NON_DISTINCT_FROM_ZERO                                    0E-6176
    DIVISION_RESULT_1A                                                       1
    DIVISION_RESULT_1B                                                       1
    SUBTRACT_RESULT_1A                                                 0E-6176
    SUBTRACT_RESULT_1B                                                 0E-6176
    SUM_CMP_RESULT_2                  <true>
    DIVISION_RESULT_2                                                        1
    SUM_AND_CMP_RESULT_2            <true>
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
