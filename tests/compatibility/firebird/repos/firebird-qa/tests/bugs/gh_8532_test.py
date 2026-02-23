#coding:utf-8

"""
ID:          issue-8532
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8532
TITLE:       GREATEST and LEAST (SQL:2023 - T054)
DESCRIPTION:
NOTES:
    [30.04.2025] pzotov
    Checked on 6.0.0.755-9d191e8 (intermediate snapshot)
    [28.12.2025] pzotov
    Changed substitutions list: value +/-0e0 can be displayed with 16 digits after decimal point.
    We have to suppress "excessive" 16th+ zeroes.
    Detected on Intel Xeon W-2123 ("Intel64 Family 6 Model 85 Stepping 4") // Windows-10
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    select least(654, 987, 123, 741) as least_01 from rdb$database;

    select least(1.976262583364986e-323, 4.940656458412465e-324, 9.881312916824931e-324) as least_02 from rdb$database;

    select least(0e0, -0e0) as least_03 from rdb$database;

    select least(true, false, false) as least_04 from rdb$database;

    select least(timestamp '01.01.0001 00:00:00.100', timestamp '31.12.9999 23:59:59.999', timestamp '01.01.1970 00:00:00.000') as least_05 from rdb$database;

    select least(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727) as least_06 from rdb$database;

    select least(df0, df1, df2) as least_07
    from (
        select
            exp( cast( -14221.4586815117860898045324562520948 as decfloat) ) as df0
           ,exp( cast( -14221.4586815117860898045324562520949 as decfloat) ) as df1
           ,exp( cast( -14221.4586815117860898045324562520950 as decfloat) ) as df2
        from rdb$database
    );


    select least(null, null, 191, null, null, -213, null, null) as least_08 from rdb$database;

    select least(
       1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 --50
      ,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 --100
      ,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 --150
      ,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 --200
      ,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 --250
      ,1,1,1,1,1 -- the last element (# 255) where result is OK
      ,1 -- adding this element leads to error message
    )  as least_09
    from rdb$database;

    ---------------------------------------------------------------------

    select greatest(654, 987, 123, 741) as greatest_01 from rdb$database;

    select greatest(1.976262583364986e-323, 4.940656458412465e-324, 9.881312916824931e-324) as greatest_02 from rdb$database;

    select greatest(0e0, -0e0) as greatest_03 from rdb$database;

    select greatest(true, false, false) as greatest_04 from rdb$database;

    select greatest(timestamp '01.01.0001 00:00:00.100', timestamp '31.12.9999 23:59:59.999', timestamp '01.01.1970 00:00:00.000') as greatest_05 from rdb$database;

    select greatest(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727) as greatest_06 from rdb$database;

    select greatest(df0, df1, df2) as greatest_07
    from (
        select
            exp( cast( -14221.4586815117860898045324562520948 as decfloat) ) as df0
           ,exp( cast( -14221.4586815117860898045324562520949 as decfloat) ) as df1
           ,exp( cast( -14221.4586815117860898045324562520950 as decfloat) ) as df2
        from rdb$database
    );


    select greatest(null, null, 191, null, null, -213, null, null) as greatest_08 from rdb$database;

    select greatest(
       1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 --50
      ,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 --100
      ,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 --150
      ,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 --200
      ,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 --250
      ,1,1,1,1,1 -- the last element (# 255) where result is OK
      ,1 -- adding this element leads to error message
    )  as greatest_09
    from rdb$database;
"""

act = isql_act('db', test_script, substitutions = [('[\t ]+', ' '), ('.0{15,}', '.000000000000000')])

expected_stdout = """
    LEAST_01                        123
    LEAST_02                                            4.940656458412465E-324
    LEAST_03                        0.000000000000000
    LEAST_04                        <false>
    LEAST_05                        0001-01-01 00:00:00.1000
    LEAST_06                             -170141183460469231731687303715884105728
    LEAST_07                                                           0E-6176
    LEAST_08                        <null>
    Statement failed, SQLSTATE = 42000
    Maximum (255) number of arguments exceeded for function LEAST

    GREATEST_01                     987
    GREATEST_02                                         1.976262583364986E-323
    GREATEST_03                     0.000000000000000
    GREATEST_04                     <true>
    GREATEST_05                     9999-12-31 23:59:59.9990
    GREATEST_06                           170141183460469231731687303715884105727
    GREATEST_07                                                        1E-6176
    GREATEST_08                     <null>
    Statement failed, SQLSTATE = 42000
    Maximum (255) number of arguments exceeded for function GREATEST
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

