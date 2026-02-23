#coding:utf-8

"""
ID:          issue-5962
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5962
TITLE:       Conversion from zero numeric literals to DECFLOAT results in incorrect value
DESCRIPTION:
    Confirmed wrong output from table with decfloat16 field (in WI-T4.0.0.1047, date of build: 03-JUL-2018).
    Confirmed overflow of decfloat34 when inserting values with exponent more than 385,
    i.e. >= 1e+385 or <= -1e385 (in WI-T4.0.0.1535, date of build: 24-JUN-2019)).
NOTES:
    Test should be added during initial migration from fbtest but did not, the reason is unknown.
    Noted by Anton Zuev: https://github.com/FirebirdSQL/firebird-qa/pull/6
    Checked on 4.0.3.2904, 5.0.0.970
FBTEST:      bugs.core_5696
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table df16(id_df16 int, val_df16 decfloat(16));
    recreate table df34(id_df34 int, val_df34 decfloat(34));
    commit;

    -- 06-jan-2018
    -- ===========
    insert into df16 values(  1, -0);
    insert into df16 values(  2, -0E300);
    insert into df16 values(  3, -0E+300);
    insert into df16 values(  4, -0.0E+300);
    insert into df16 values(  5, 0E+300);
    insert into df16 values(  6, 0E-300);

    -- 22-jun-2019
    -- ===========
    insert into df16 values(  7, 0e+370);
    insert into df16 values(  8, 0e-399);
    insert into df16 values(  9, 0e+6111);
    insert into df16 values( 10, 0e-6167);

    insert into df16 values( 11, 1e-6176);
    insert into df16 values( 12, 1e+6111); -- must raise SQLSTATE = 22003 Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.
    insert into df16 values( 13, 1e+6144); -- must raise SQLSTATE = 22003 Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.
    insert into df16 values( 14, 1.234567890123456789012345678901234E0);
    insert into df16 values( 15, 9999999999999999E+369);

    select t.* from df16 t;

    ----------------------------------------------

    -- 22-jun-2019
    -- ===========
    insert into df34 values(  1, 0e+370);
    insert into df34 values(  2, 0e-399);
    insert into df34 values(  3, 0e+6111);
    insert into df34 values(  4, 0e-6167);
    insert into df34 values(  5, 1e-6176);
    insert into df34 values(  6, 1e385);  -- DID raise overflow in WI-T4.0.0.1535, i.e. before this ticket was fixed; ticket sample: 1e+6111
    insert into df34 values(  7, -1e385); -- DID raise overflow in WI-T4.0.0.1535, i.e. before this ticket was fixed; ticket sample: 1e+6144
    insert into df34 values(  8, 1e+6111);
    insert into df34 values(  9, 1.234567890123456789012345678901234E0);
                              
    select t.* from df34 t;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    ID_DF16                         1
    VAL_DF16                                             -0

    ID_DF16                         2
    VAL_DF16                                        -0E+300

    ID_DF16                         3
    VAL_DF16                                        -0E+300

    ID_DF16                         4
    VAL_DF16                                        -0E+299

    ID_DF16                         5
    VAL_DF16                                         0E+300

    ID_DF16                         6
    VAL_DF16                                         0E-300

    ID_DF16                         7
    VAL_DF16                                         0E+369

    ID_DF16                         8
    VAL_DF16                                         0E-398

    ID_DF16                         9
    VAL_DF16                                         0E+369

    ID_DF16                         10
    VAL_DF16                                         0E-398

    ID_DF16                         11
    VAL_DF16                                         0E-398

    ID_DF16                         14
    VAL_DF16                              1.234567890123457

    ID_DF16                         15
    VAL_DF16                         9.999999999999999E+384



    ID_DF34                         1
    VAL_DF34                                                            0E+370

    ID_DF34                         2
    VAL_DF34                                                            0E-399

    ID_DF34                         3
    VAL_DF34                                                           0E+6111

    ID_DF34                         4
    VAL_DF34                                                           0E-6167

    ID_DF34                         5
    VAL_DF34                                                           1E-6176

    ID_DF34                         6
    VAL_DF34                                                           1E+385

    ID_DF34                         7
    VAL_DF34                                                          -1E+385

    ID_DF34                         8
    VAL_DF34                                                           1E+6111

    ID_DF34                         9
    VAL_DF34                               1.234567890123456789012345678901234
"""

expected_stderr = """
    Statement failed, SQLSTATE = 22003
    Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.

    Statement failed, SQLSTATE = 22003
    Decimal float overflow.  The exponent of a result is greater than the magnitude allowed.
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout and act.clean_stderr == act.clean_expected_stderr
