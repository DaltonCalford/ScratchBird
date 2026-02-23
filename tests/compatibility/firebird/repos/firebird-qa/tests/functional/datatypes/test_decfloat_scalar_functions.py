#coding:utf-8

"""
ID:          decfloat.scalar-functions
ISSUE:       5803
JIRA:        CORE-5535
TITLE:       Test common math functions that should work with DECFLOAT datatype
DESCRIPTION:
  See  doc/sql.extensions/README.data_types
    A number of standard functions can be used with DECFLOAT datatype. It is:
    ABS, CEILING, EXP, FLOOR, LN, LOG, LOG10, POWER, SIGN, SQRT.
NOTES:
[31.10.2019]
  adjusted output to recent FB version. Checked on 4.0.0.1635 SS: 0.917s.
[26.06.2020]
  adjusted output to recent FB version. Checked on 4.0.0.2079, intermediate snapshot with
  timestamp = 26.06.2020 14:34.
[21.08.2020]
  put literal numeric values into a table with DECFLOAT table; replaced UNIONED-code with
  separate statements. Checked on 4.0.0.2173
FBTEST:      functional.datatypes.decfloat_scalar_functions
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate table test(n1 decfloat);
    insert into test(n1) values (-123456789012345678901234567890.123);
    commit;
    select abs(n1) as abs_x from test;
    select ceiling(n1) as ceiling_x from test;
    select floor(n1) as floor_x from test;
    select ceiling(abs(n1)) as ceil_abs_x from test;
    select floor(abs(n1)) as floor_abs_x from test;
    commit;
    -------------------------------------------------------------------
    delete from test;
    insert into test(n1) values (
    9999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999
    );
    select ln(n1) as ln_1e1024 from test;
    select exp(ln(n1)) as exp_1e1024 from test;
    select log10(n1) as log10_1e1024 from test;
    select power(n1,-1.0000000000/log10(n1)) as power_x from test;
    select sqrt(n1) as sqrt_1e1024 from test;
    select sign(-n1) as sign_1e1024 from test;
    commit;
    -------------------------------------------------------------------
    recreate table test(power_2_127_dec_1 decfloat, power_2_127_exact decfloat);
    insert into test(power_2_127_dec_1, power_2_127_exact) values (
                 170141183460469231731687303715884105727
                ,170141183460469231731687303715884105728
    );
    select ln(power_2_127_dec_1) as ln1, ln(power_2_127_exact) as ln2 from test;

    select exp(ln(power_2_127_dec_1)) as exp1, exp(ln(power_2_127_dec_1)) as exp2 from test;

    select log10(power_2_127_dec_1) as log10a, log10(power_2_127_exact) as log10b from test;

    select power(power_2_127_dec_1,-1.0000000000 / log10(power_2_127_dec_1)) as power_a
          ,power(power_2_127_exact,-1.0000000000/log10(power_2_127_exact)) as power_b
    from test;

    select sqrt(power_2_127_dec_1) as sqrt_a, sqrt(power_2_127_exact) as sqrt_b from test;

    select sign(-power_2_127_dec_1) as sign_a, sign(-power_2_127_exact) as sign_b from test;


"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    ABS_X                                   123456789012345678901234567890.123
    CEILING_X                                  -123456789012345678901234567890
    FLOOR_X                                    -123456789012345678901234567891
    CEIL_ABS_X                                  123456789012345678901234567891
    FLOOR_ABS_X                                 123456789012345678901234567890
    LN_1E1024                              2357.847135225902780434423249596789
    EXP_1E1024                       9.999999999999999999999999999996197E+1023
    LOG10_1E1024                                                          1024
    POWER_X                               0.1000000000000000000000000000000000
    SQRT_1E1024                                       1.00000000000000000E+512
    SIGN_1E1024                     -1
    LN1                                    88.02969193111305429598847942518842
    LN2                                    88.02969193111305429598847942518842
    EXP1                               1.701411834604692317316873037158807E+38
    EXP2                               1.701411834604692317316873037158807E+38
    LOG10A                                 38.23080944932561179214483963001061
    LOG10B                                 38.23080944932561179214483963001061
    POWER_A                              0.09999999999999999999999999999999995
    POWER_B                              0.09999999999999999999999999999999995
    SQRT_A                                 13043817825332782212.34957180625251
    SQRT_B                                 13043817825332782212.34957180625251
    SIGN_A                          -1
    SIGN_B                          -1
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
