#coding:utf-8

"""
ID:          int128.math-functions
ISSUE:       6585
JIRA:        CORE-6344
TITLE:       Basic test for math functions against INT128 datatype
DESCRIPTION:
  Test verifies https://github.com/FirebirdSQL/firebird/commit/57551c3bc0f348306ac10917cb4cc862886c88c5
  (Postfix for #6585 - fixed ROUND() & TRUNC()).

  Also it checks result of all other math functions that can be applied to boundary values of INT128 datatype.
  This test is used instead of separate test for #6585 in order to verify all math-functions to in one file.

  Some expression still can not be evaluated and produce errors - they are commented (see "deferred" here).
  See notes in https://github.com/FirebirdSQL/firebird/issues/6585
FBTEST:      functional.datatypes.int128_math_functions
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table test(i128_least int128 , i128_great int128);
    insert into test(i128_least, i128_great) values(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727);

    select i128_least / i128_great as math_division from test;

    select i128_least + i128_great as math_addition from test;

    select mod(i128_least, i128_great) as math_mod_a from test;

    select mod(i128_great, i128_least) as math_mod_b from test;

    select abs(i128_great) as math_abs from test;

    select ceil(i128_least) as math_ceil_a from test;
    select ceil(i128_great) as math_ceil_b from test;

    select floor(i128_least) as math_floor_a from test;
    select floor(i128_great) as math_floor_b from test;

    select log(2,i128_great) as math_log_base2 from test; -- expected: 127 (approx)

    -- wolfram: 88.02969193111305429598847942518842414558263959199830583578...
    select ln(i128_great) as math_ln from test;


    -- wolfram: 38.2308094493256117921448396300106143995575623921374319229378881916007690214721499090149518611002670672485111597704...
    select log10(i128_great) as math_log_base10 from test;

    select round(i128_least,0) as math_round_a from test;

    select round(i128_great,0) as math_round_b from test;

    -- deferred, currently fails: select round(i128_least,-128) as math_round_a from test;

    select round(i128_least,-127) as math_round_c from test;

    -- deferred, currently fails: select round(i128_great, 1) as math_round_d from test;

    select round(170141183460469231731687303715884105727,0) from rdb$database;

    /*
    deferred: currently these all fail:
    select round(170141183460469231731687303715884105727,1) from rdb$database;
    select round(-170141183460469231731687303715884105728,0) from rdb$database;
    select round(-170141183460469231731687303715884105728,1) from rdb$database;
    */

    select sign(i128_least) as math_sign_a from test;

    select sign(i128_great) as math_sign_b from test;

    select sqrt(i128_great) as math_sqrt from test;

    -- fails: select trunc(i128_least, -128) from test;
    select trunc(i128_least, -127) as math_trunc_a from test;

    select trunc(i128_great, 127) as math_trunc_b from test;

    -- value from wolfram: 88.7228391116729996054057115466466007136581397263585610899071975324682723837318043612609801346001720877892090831370...
    select acosh(i128_great) as math_acosh from test;

    select asinh(i128_great) as math_asinh from test;

    -- wolfram: 1.570796326794896619231321691639751442092707227933441472947
    select atan(i128_great) as math_atan from test;

    -- wolfram: -0.78539816339744830961566084581987572105223108572083217401...
    select atan2(i128_least, i128_great) as math_atan2 from test;

    -- wolfram: 0.947031103791698176468465227552673769940449295269499764416...
    -- RETURNS WRONG RESULT! >>> select cos(i128_great) from test;

    -- 88.0296919311130542959884794251884241455826395919983058357865175229748787617621182818236719020483787865990704025160...
    --   1.7014118346046923071280032718565510658506726293419382...*10E38 -- value for exp(88.02969193111305429) in wolfram
    select cast( exp(88.02969193111305429) as int128 ) as math_exp_cast_to_int128 from test;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    MATH_DIVISION                                                              -1
    MATH_ADDITION                                                              -1
    MATH_MOD_A                                                                 -1
    MATH_MOD_B                            170141183460469231731687303715884105727
    MATH_ABS                              170141183460469231731687303715884105727
    MATH_CEIL_A                          -170141183460469231731687303715884105728
    MATH_CEIL_B                           170141183460469231731687303715884105727
    MATH_FLOOR_A                         -170141183460469231731687303715884105728
    MATH_FLOOR_B                          170141183460469231731687303715884105727
    MATH_LOG_BASE2                         127.0000000000000000000000000000000
    MATH_LN                                88.02969193111305429598847942518842
    MATH_LOG_BASE10                        38.23080944932561179214483963001061
    MATH_ROUND_A                         -170141183460469231731687303715884105728
    MATH_ROUND_B                          170141183460469231731687303715884105727
    MATH_ROUND_C                                                                0
    ROUND                                 170141183460469231731687303715884105727
    MATH_SIGN_A                     -1
    MATH_SIGN_B                     1
    MATH_SQRT                              13043817825332782212.34957180625251
    MATH_TRUNC_A                                                                0
    MATH_TRUNC_B                          170141183460469231731687303715884105727
    MATH_ACOSH                      88.72283911167300
    MATH_ASINH                      88.72283911167300
    MATH_ATAN                       1.570796326794897
    MATH_ATAN2                      -0.7853981633974483
    MATH_EXP_CAST_TO_INT128               170141183460468400595186318658326495232
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
