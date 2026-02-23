#coding:utf-8

"""
ID:          decfloat.ordering-rules
TITLE:       Test ordering of decfloat values
DESCRIPTION:
    Test verifies work of:
        - COMPARE_DECFLOAT - compares two DECFLOAT values to be equal, different or unordered.
            Returns SMALLINT value which can be as follows:
            0 - values are equal
            1 - first value is less than second
            2 - first value is greater than second
            3 - values unordered (i.e. one or both is NAN / SNAN)
        Unlike comparison operators ('<', '=', '>', etc.) comparison is exact - i.e.
            COMPARE_DECFLOAT(2.17, 2.170) returns 2, not 0.
    
        - TOTALORDER - compares two DECFLOAT values including any special value. The comparison is exact.
            Returns SMALLINT value which can be as follows:
            -1 - first value is less than second
            0 - values are equal
            1 - first value is greater than second

        DECFLOAT values are ordered as follows:
        -nan < -snan < -inf < -0.1 < -0.10 < -0 < 0 < 0.10 < 0.1 < inf < snan < nan

    Checked on 4.0.0.1714.
FBTEST:      functional.datatypes.decfloat_ordering_rules
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set decfloat traps to;
    select
         t.*
        ,decode( totalorder("-nan", "-snan"),  -1, '-nan LSS -snan',   1, '-nan GTR -snan',   0, '-nan EQU -snan', 'UNKNOWN')  as "totalorder(-nan, -snan)"
        ,decode( totalorder("-snan", "-inf"),  -1, '-snan LSS -inf',   1, '-snan GTR -inf',   0, '-snan EQU -inf', 'UNKNOWN')  as "totalorder(-snan, -inf)"
        ,decode( totalorder("-inf", "-0.1"),   -1, '-inf LSS -0.1',    1, '-inf GTR -0.1',    0, '-inf EQU -0.1', 'UNKNOWN')   as "totalorder(-inf, -0.1)"
        ,decode( totalorder("-0.1", "-0.10"),  -1, '-0.1 LSS -0.10',   1, '-0.1 GTR -0.10',   0, '-0.1 EQU -0.10', 'UNKNOWN')  as "totalorder(-0.1, -0.10)"
        ,decode( totalorder("-0.10", "-0"),    -1, '-0.10 LSS -0',     1, '-0.10 GTR -0',     0, '-0.10 EQU -0', 'UNKNOWN')    as "totalorder(-0.10, -0)"
        ,decode( totalorder("-0", "0"),        -1, '-0 LSS 0',         1, '-0 GTR 0',         0, '-0 EQU 0', 'UNKNOWN')        as "totalorder(-0, 0)"
        ,decode( totalorder("0", "0.10"),      -1, '0 LSS 0.10',       1, '0 GTR 0.10',       0, '0 EQU 0.10', 'UNKNOWN')      as "totalorder(0, 0.10)"
        ,decode( totalorder("0.10", "0.1"),    -1, '0.10 LSS 0.1',     1, '0.10 GTR 0.1',     0, '0.10 EQU 0.1', 'UNKNOWN')    as "totalorder(0.10, 0.1)"
        ,decode( totalorder("0.1", "inf"),     -1, '0.1 LSS inf',      1, '0.1 GTR inf',      0, '0.1 EQU inf', 'UNKNOWN')     as "totalorder(0.1, inf)"
        ,decode( totalorder("inf", "snan"),    -1, 'inf LSS snan',     1, 'inf GTR snan',     0, 'inf EQU snan', 'UNKNOWN')    as "totalorder(inf, snan)"
        ,decode( totalorder("snan", "nan"),    -1, 'snan LSS nan',     1, 'snan GTR nan',     0, 'snan EQU nan', 'UNKNOWN')    as "totalorder(snan, nan)"
        
        ,decode( compare_decfloat("-nan", "-snan"),   1, '-nan LSS -snan',   2, '-nan GTR -snan',   0, '-nan EQU -snan', 3, 'Unordered',  'UNKNOWN' ) as "compare_decfloat(-nan, -snan)"
        ,decode( compare_decfloat("-snan", "-inf"),   1, '-snan LSS -inf',   2, '-snan GTR -inf',   0, '-snan EQU -inf', 3, 'Unordered',  'UNKNOWN' ) as "compare_decfloat(-snan, -inf)"
        ,decode( compare_decfloat("-inf", "-0.1"),   1, '-inf LSS -0.1',   2, '-inf GTR -0.1',   0, '-inf EQU -0.1', 3, 'Unordered',  'UNKNOWN' ) as "compare_decfloat(-inf, -0.1)"
        ,decode( compare_decfloat("-0.1", "-0.10"),   1, '-0.1 LSS -0.10',   2, '-0.1 GTR -0.10',   0, '-0.1 EQU -0.10', 3, 'Unordered',  'UNKNOWN' ) as "compare_decfloat(-0.1, -0.10)"
        ,decode( compare_decfloat("-0.10", "-0"),   1, '-0.10 LSS -0',   2, '-0.10 GTR -0',   0, '-0.10 EQU -0', 3, 'Unordered',  'UNKNOWN' ) as "compare_decfloat(-0.10, -0)"
        ,decode( compare_decfloat("-0", "0"),   1, '-0 LSS 0',   2, '-0 GTR 0',   0, '-0 EQU 0', 3, 'Unordered',  'UNKNOWN' ) as "compare_decfloat(-0, 0)"
        ,decode( compare_decfloat("0", "0.10"),   1, '0 LSS 0.10',   2, '0 GTR 0.10',   0, '0 EQU 0.10', 3, 'Unordered',  'UNKNOWN' ) as "compare_decfloat(0, 0.10)"
        ,decode( compare_decfloat("0.10", "0.1"),   1, '0.10 LSS 0.1',   2, '0.10 GTR 0.1',   0, '0.10 EQU 0.1', 3, 'Unordered',  'UNKNOWN' ) as "compare_decfloat(0.10, 0.1)"
        ,decode( compare_decfloat("0.1", "inf"),   1, '0.1 LSS inf',   2, '0.1 GTR inf',   0, '0.1 EQU inf', 3, 'Unordered',  'UNKNOWN' ) as "compare_decfloat(0.1, inf)"
        ,decode( compare_decfloat("inf", "snan"),   1, 'inf LSS snan',   2, 'inf GTR snan',   0, 'inf EQU snan', 3, 'Unordered',  'UNKNOWN' ) as "compare_decfloat(inf, snan)"
        ,decode( compare_decfloat("snan", "nan"),   1, 'snan LSS nan',   2, 'snan GTR nan',   0, 'snan EQU nan', 3, 'Unordered',  'UNKNOWN' ) as "compare_decfloat(snan, nan)"

        ,iif( "-nan" < "-snan", '-nan LSS -snan', iif( "-nan" > "-snan", '-nan GTR -snan', iif("-nan" = "-snan", '-nan EQU -snan', 'UNKNOWN') ) ) as "arithmetic: -nan vs to -snan:"
        ,iif( "-snan" < "-inf", '-snan LSS -inf', iif( "-snan" > "-inf", '-snan GTR -inf', iif("-snan" = "-inf", '-snan EQU -inf', 'UNKNOWN') ) ) as "arithmetic: -snan vs to -inf:"
        ,iif( "-inf" < "-0.1", '-inf LSS -0.1', iif( "-inf" > "-0.1", '-inf GTR -0.1', iif("-inf" = "-0.1", '-inf EQU -0.1', 'UNKNOWN') ) ) as "arithmetic: -inf vs to -0.1:"
        ,iif( "-0.1" < "-0.10", '-0.1 LSS -0.10', iif( "-0.1" > "-0.10", '-0.1 GTR -0.10', iif("-0.1" = "-0.10", '-0.1 EQU -0.10', 'UNKNOWN') ) ) as "arithmetic: -0.1 vs to -0.10:"
        ,iif( "-0.10" < "-0", '-0.10 LSS -0', iif( "-0.10" > "-0", '-0.10 GTR -0', iif("-0.10" = "-0", '-0.10 EQU -0', 'UNKNOWN') ) ) as "arithmetic: -0.10 vs to -0:"
        ,iif( "-0" < "0", '-0 LSS 0', iif( "-0" > "0", '-0 GTR 0', iif("-0" = "0", '-0 EQU 0', 'UNKNOWN') ) ) as "arithmetic: -0 vs to 0:"
        ,iif( "0" < "0.10", '0 LSS 0.10', iif( "0" > "0.10", '0 GTR 0.10', iif("0" = "0.10", '0 EQU 0.10', 'UNKNOWN') ) ) as "arithmetic: 0 vs to 0.10:"
        ,iif( "0.10" < "0.1", '0.10 LSS 0.1', iif( "0.10" > "0.1", '0.10 GTR 0.1', iif("0.10" = "0.1", '0.10 EQU 0.1', 'UNKNOWN') ) ) as "arithmetic: 0.10 vs to 0.1:"
        ,iif( "0.1" < "inf", '0.1 LSS inf', iif( "0.1" > "inf", '0.1 GTR inf', iif("0.1" = "inf", '0.1 EQU inf', 'UNKNOWN') ) ) as "arithmetic: 0.1 vs to inf:"
        ,iif( "inf" < "snan", 'inf LSS snan', iif( "inf" > "snan", 'inf GTR snan', iif("inf" = "snan", 'inf EQU snan', 'UNKNOWN') ) ) as "arithmetic: inf vs to snan:"
        ,iif( "snan" < "nan", 'snan LSS nan', iif( "snan" > "nan", 'snan GTR nan', iif("snan" = "nan", 'snan EQU nan', 'UNKNOWN') ) ) as "arithmetic: snan vs to nan:"
    from (
        select
            -cast('foo' as decfloat) as "-nan"
            ,-cast('snan' as decfloat) as "-snan"
            ,-cast(1/1e-9999 as decfloat) as "-inf"
            ,-cast(0.1 as decfloat) as "-0.1"
            ,-cast(0.10 as decfloat) as "-0.10"
            ,-cast(0 as decfloat) as "-0"
            ,cast(0 as decfloat) as "0"
            ,cast(0.10 as decfloat) as "0.10"
            ,cast(0.1 as decfloat) as "0.1"
            ,cast(1/1e-9999 as decfloat) as "inf"
            ,cast('snan' as decfloat) as "snan"
            ,cast('bar' as decfloat) as "nan"
        from rdb$database
    ) t;
"""

act = isql_act('db', test_script, substitutions=[('[\\s]+', ' ')])

expected_stdout = """
    -nan                                                                  -NaN
    -snan                                                                -sNaN
    -inf                                                             -Infinity
    -0.1                                                                  -0.1
    -0.10                                                                -0.10
    -0                                                                      -0
    0                                                                        0
    0.10                                                                  0.10
    0.1                                                                    0.1
    inf                                                               Infinity
    snan                                                                  sNaN
    nan                                                                    NaN

    totalorder(-nan, -snan)         -nan LSS -snan
    totalorder(-snan, -inf)         -snan LSS -inf
    totalorder(-inf, -0.1)          -inf LSS -0.1
    totalorder(-0.1, -0.10)         -0.1 LSS -0.10
    totalorder(-0.10, -0)           -0.10 LSS -0
    totalorder(-0, 0)               -0 LSS 0
    totalorder(0, 0.10)             0 LSS 0.10
    totalorder(0.10, 0.1)           0.10 LSS 0.1
    totalorder(0.1, inf)            0.1 LSS inf
    totalorder(inf, snan)           inf LSS snan
    totalorder(snan, nan)           snan LSS nan

    compare_decfloat(-nan, -snan)   Unordered
    compare_decfloat(-snan, -inf)   Unordered
    compare_decfloat(-inf, -0.1)    -inf LSS -0.1
    compare_decfloat(-0.1, -0.10)   -0.1 LSS -0.10
    compare_decfloat(-0.10, -0)     -0.10 LSS -0
    compare_decfloat(-0, 0)         -0 LSS 0
    compare_decfloat(0, 0.10)       0 LSS 0.10
    compare_decfloat(0.10, 0.1)     0.10 LSS 0.1
    compare_decfloat(0.1, inf)      0.1 LSS inf
    compare_decfloat(inf, snan)     Unordered
    compare_decfloat(snan, nan)     Unordered

    arithmetic: -nan vs to -snan:   -nan EQU -snan
    arithmetic: -snan vs to -inf:   -snan EQU -inf
    arithmetic: -inf vs to -0.1:    -inf LSS -0.1
    arithmetic: -0.1 vs to -0.10:   -0.1 EQU -0.10
    arithmetic: -0.10 vs to -0:     -0.10 LSS -0
    arithmetic: -0 vs to 0:         -0 EQU 0
    arithmetic: 0 vs to 0.10:       0 LSS 0.10
    arithmetic: 0.10 vs to 0.1:     0.10 EQU 0.1
    arithmetic: 0.1 vs to inf:      0.1 LSS inf
    arithmetic: inf vs to snan:     inf EQU snan
    arithmetic: snan vs to nan:     snan EQU nan
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
