#coding:utf-8
"""
ID:          issue-7810
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7810
TITLE:       Improve SKIP LOCKED implementation
DESCRIPTION:
    Test verifies ability to CHANGE records in a table which already has several locked rows by another transaction.
    Special attention is paid to COMPUTED columns: their values must present in the output which show updated/deleted data.
    Three tables are created with numeric, textual and time_zone fields. Each table contain three COMPUTED columns (e01, e02 and e03).
    Computed column 'e02' is based on 'e01', and 'e03' is based on 'e02'.
    Formulas for computed columns were choosen so that column 'e02' must have value equals to non-computed column or 'e01'.
    Test checks that value of 'e02' that is returned after update/delete must be equal to 'source' column (see 'e03' values in the output).
NOTES:
    1. Parameter ReadConsistency in firebird.conf must be set to 0, i.e. NOT-default value.
    2. TIL = "RC NO_record_version" can be used to check feature since gh-7811 was fixed (20.11.2023 in master); see 'tpb_isol_set'
    3. Here we check only UPDATE and DELETE behavior.
       Ability to run 'SELECT ... WITH LOCK' is checked in gh_7350_test.py
       https://github.com/FirebirdSQL/firebird/issues/7350

    Checked on 6.0.0.150, 5.0.0.1280 (intermediate build 27.11.2023, after backporting, commit dc2d85c17b41fb6c378bffc0896338c4f8856998).
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, TraLockResolution, TraAccessMode, DatabaseError
import time

db = db_factory(charset = 'utf8')
act = python_act('db', substitutions = [('[ \t]+', ' ')])

expected_stdout = f"""
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_num set id = id order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_num order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_num set id = id order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_num order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_num order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_num order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_txt set id = id order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_txt order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_txt set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_txt order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_tz set id = id order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_tz order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_tz set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_tz order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = NO_WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_num set id = id order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_num order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_num set id = id order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_num order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_num order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_num order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_txt set id = id order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_txt order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_txt set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_txt order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_tz set id = id order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_tz order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_tz set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_tz order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = SNAPSHOT, WAIT = WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_num set id = id order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_num order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_num set id = id order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_num order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_num order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_num order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_txt set id = id order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_txt order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_txt set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_txt order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_tz set id = id order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_tz order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_tz set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_tz order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = NO_WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_num set id = id order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_num order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_num set id = id order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_num order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_num order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_num order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_txt set id = id order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_txt order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_txt set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_txt order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_tz set id = id order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_tz order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_tz set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_tz order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_READ_CONSISTENCY, WAIT = WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_num set id = id order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_num order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_num set id = id order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_num order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_num order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_num order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_txt set id = id order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_txt order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_txt set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_txt order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_tz set id = id order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_tz order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_tz set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_tz order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = NO_WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_num set id = id order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_num order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_num set id = id order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_num order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_num order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_num order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_txt set id = id order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_txt order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_txt set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_txt order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_tz set id = id order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_tz order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_tz set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_tz order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_RECORD_VERSION, WAIT = WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_num set id = id order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_num order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_num set id = id order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_num order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_num order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_num order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_txt set id = id order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_txt order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_txt set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_txt order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_tz set id = id order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_tz order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_tz set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_tz order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = NO_WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_num set id = id order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_num order by id rows 7 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E01:                                                             2
    E02:                                                             4
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E01:                                                             3
    E02:                                                             9
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_num set id = id order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_num order by id rows 7 skip locked returning *;
    ID:                                                              -2
    N01:                                                             -9223372036854775808
    E00:                                                             8.507059173023461586584365185794205E+37
    E01:                                                             8.507059173023461586584365185794205E+37
    E02:                                                             -9223372036854775808.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              -1
    N01:                                                             9223372036854775807
    E00:                                                             8.507059173023461584739690778423250E+37
    E01:                                                             8.507059173023461584739690778423250E+37
    E02:                                                             9223372036854775807.000000000000000
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              2
    N01:                                                             4
    E00:                                                             16
    E01:                                                             2
    E02:                                                             4
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              3
    N01:                                                             9
    E00:                                                             81
    E01:                                                             3
    E02:                                                             9
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_num order by id rows 5 to 8 skip locked returning id,n01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              4
    N01:                                                             16
    E01:                                                             4
    E02:                                                             16
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E01:                                                             6
    E02:                                                             36
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E01:                                                             8
    E02:                                                             64
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E01:                                                             10
    E02:                                                             100
    E03_RET:                                                         Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_num set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_num order by id rows 5 to 8 skip locked returning *;
    ID:                                                              4
    N01:                                                             16
    E00:                                                             256
    E01:                                                             4
    E02:                                                             16
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              6
    N01:                                                             36
    E00:                                                             1296
    E01:                                                             6
    E02:                                                             36
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              8
    N01:                                                             64
    E00:                                                             4096
    E01:                                                             8
    E02:                                                             64
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    ID:                                                              10
    N01:                                                             100
    E00:                                                             10000
    E01:                                                             10
    E02:                                                             100
    E03:                                                             Is e02 not distinct from n01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_txt set id = id order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_txt order by id rows 7 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_txt set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_txt order by id rows 7 skip locked returning *;
    ID:                                                              2
    S01:                                                             qwe2
    B01:                                                             rty2
    E01:                                                             qwe2_rty2
    E02:                                                             qwe2_rty2
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              3
    S01:                                                             qwe3
    B01:                                                             rty3
    E01:                                                             qwe3_rty3
    E02:                                                             qwe3_rty3
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              4
    S01:                                                             qwe4
    B01:                                                             rty4
    E01:                                                             qwe4_rty4
    E02:                                                             qwe4_rty4
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              6
    S01:                                                             qwe6
    B01:                                                             rty6
    E01:                                                             qwe6_rty6
    E02:                                                             qwe6_rty6
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning id,s01,b01, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03_RET:                                                         Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_txt set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_txt order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    S01:                                                             qwe8
    B01:                                                             rty8
    E01:                                                             qwe8_rty8
    E02:                                                             qwe8_rty8
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    ID:                                                              10
    S01:                                                             qwe10
    B01:                                                             rty10
    E01:                                                             qwe10_rty10
    E02:                                                             qwe10_rty10
    E03:                                                             Is e02 not distinct from e01 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_tz set id = id order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_tz order by id rows 7 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_tz set id = id order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_tz order by id rows 7 skip locked returning *;
    ID:                                                              2
    T0:                                                              12:34:59
    TZ:                                                              America/Barbados
    E01:                                                             12:34:59
    E02:                                                             12:34:59
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              3
    T0:                                                              12:35:00
    TZ:                                                              America/Barbados
    E01:                                                             12:35:00
    E02:                                                             12:35:00
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              4
    T0:                                                              12:35:01
    TZ:                                                              America/Barbados
    E01:                                                             12:35:01
    E02:                                                             12:35:01
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              6
    T0:                                                              12:35:03
    TZ:                                                              America/Barbados
    E01:                                                             12:35:03
    E02:                                                             12:35:03
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning id,t0,tz, e01, e02, e03 collate unicode_ci as e03_ret;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03_RET:                                                         Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: update test_tz set id = id order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    TIL = READ_COMMITTED_NO_RECORD_VERSION, WAIT = WAIT: delete from test_tz order by id rows 5 to 8 skip locked returning *;
    ID:                                                              8
    T0:                                                              12:35:05
    TZ:                                                              America/Barbados
    E01:                                                             12:35:05
    E02:                                                             12:35:05
    E03:                                                             Is e02 not distinct from t0 ? ==> true
    ID:                                                              10
    T0:                                                              12:35:07
    TZ:                                                              America/Barbados
    E01:                                                             12:35:07
    E02:                                                             12:35:07
    E03:                                                             Is e02 not distinct from t0 ? ==> true
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):

    init_sql = f"""
        recreate table test_num(
            id int primary key
            ,n01 decfloat
            ,e00 computed by ( n01 * n01 )
            ,e01 computed by ( iif(id <= 0, n01 * n01, sqrt(n01)) )
            ,e02 computed by ( iif(id <= 0, sign(n01) * sqrt(e01), e01 * e01) )
            ,e03 computed by ( 'Is e02 not distinct from n01 ? ==> ' || iif(e02 = n01, 'true', 'false') )
        );

        recreate table test_txt(
            id int primary key
            ,s01 varchar(10)
            ,b01 blob sub_type text
            ,e01 computed by ( blob_append(s01, '_', b01) )
            ,e02 computed by ( blob_append( substring(e01 from 1 for position('_' in e01)-1), '_', substring(e01 from position('_' in e01)+1) ) )
            ,e03 computed by ( 'Is e02 not distinct from e01 ? ==> ' || iif(e02 = e01, 'true', 'false') )
        );

        recreate table test_tz(
            id int primary key
            ,t0 time with time zone
            ,tz varchar(50)
            ,e01 computed by ( cast( cast(t0 as time) || ' '  || tz as time with time zone) )
            ,e02 computed by (
                cast(
                    substring(
                        cast(e01 as varchar(255))
                        from 1 for position(' ' in e01)-1
                    )
                    || ' '
                    || substring( cast(t0 as varchar(255)) from position(' ' in cast(t0 as varchar(255)))+1 )
                    as time with time zone
                )
            )
            ,e03 computed by ( 'Is e02 not distinct from t0 ? ==> ' || iif(e02 = t0, 'true', 'false') )
        );

        commit;
        insert into test_num(id, n01) values(-2, -9223372036854775808);
        insert into test_num(id, n01) values(-1,  9223372036854775807);
        insert into test_num(id, n01) select row_number()over(), row_number()over() * row_number()over() from rdb$types rows 10;
        commit;
        insert into test_txt(id, s01, b01) select row_number()over(), 'qwe' || row_number()over(), 'rty' || row_number()over() from rdb$types rows 10;
        commit;
        insert into test_tz(id, t0, tz) select row_number()over(), dateadd(row_number()over() second to cast('12:34:57' as time with time zone)), 'America/Barbados' from rdb$types rows 10;
        commit;
    """ 
    act.isql(switches=['-q'], input = init_sql, combine_output = True)
    assert act.clean_stdout == ''
    act.reset()

    #tpb_isol_set = (Isolation.SERIALIZABLE, Isolation.SNAPSHOT, Isolation.READ_COMMITTED_READ_CONSISTENCY, Isolation.READ_COMMITTED_RECORD_VERSION, Isolation.READ_COMMITTED_NO_RECORD_VERSION)
    tpb_isol_set = (Isolation.SNAPSHOT, Isolation.READ_COMMITTED_READ_CONSISTENCY, Isolation.READ_COMMITTED_RECORD_VERSION, Isolation.READ_COMMITTED_NO_RECORD_VERSION)
    tpb_wait_set = (TraLockResolution.NO_WAIT,TraLockResolution.WAIT)
    tpb_mode_set = (TraAccessMode.READ, TraAccessMode.WRITE)
    query_types_set = ('DSQL',) # 'PSQL_LOCAL', 'PSQL_REMOTE')

    with act.db.connect() as con_rows_locker, act.db.connect() as con_free_seeker:
        cur_aux = con_rows_locker.cursor()

        dml_list = []
        for tab_name in ('test_num', 'test_txt', 'test_tz'):
            get_non_computed_col_sql = f"""
                select
                    trim(lower(rf.rdb$field_name)) as fld_name
                from rdb$relation_fields rf
                join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
                where
                    rf.rdb$relation_name = '{tab_name.upper()}'
                    and f.rdb$computed_blr is null
                order by rf.rdb$field_position
            """
            cur_aux.execute(get_non_computed_col_sql)
            non_computed_cols_list = []
            for r in cur_aux:
                non_computed_cols_list.append(r[0])

            all_columns_to_return = ','.join(non_computed_cols_list) + ', e01, e02, e03 collate unicode_ci as e03_ret'

            con_rows_locker.execute_immediate(f'update {tab_name} set id = id where id in (1,5,7,9)')
            # NB: after rows with ID = {1,5,7,9} are locked:
            #     select * from test order by id rows 7 with lock skip locked; -- must issue 6 rows: {2,3,4,6,8,10}
            #     select * from ... order by id rows 5 to 8 with lock skip locked; -- must issue 2 rows: {8, 10}

            for rows_clause in ('rows 7', 'rows 5 to 8'):
                for ret_clause in ( f'returning {all_columns_to_return}', 'returning *'):
                    dml_list.append(f'update {tab_name} set id = id order by id {rows_clause} skip locked {ret_clause};')
                    dml_list.append(f'delete from {tab_name} order by id {rows_clause} skip locked {ret_clause};')


        for x_isol in tpb_isol_set:
            for x_wait in tpb_wait_set:
                if x_isol in (Isolation.SERIALIZABLE,) and x_wait == TraLockResolution.WAIT:
                    
                    #######################################
                    ###    D O    N O T    C H E C K    ###
                    #######################################
                    #
                    # 1. Isolation.SERIALIZABLE requires that the whole table must not be changed by anyone else.
                    # 2. [WAS ACTUAL BEFORE GH-7810 FIXED; CURRENTLY REMAINS NEEDED FB 5.X]
                    #    Isolation.READ_COMMITTED_NO_RECORD_VERSION can not be used due to implementation details, see:
                    #    Adriano, 26-feb-2023, https://github.com/FirebirdSQL/firebird/pull/7350#issuecomment-1445408462
                    #    "WITH LOCK [SKIP LOCKED] needs a record read before, but this locked records cannot be read with NO RECORD VERSION.
                    #    Considering that this transaction mode is replaced by default I would only document it as in fact I don't think
                    #    there are anything we could do."

                    continue

                custom_tpb = tpb(isolation = x_isol, lock_timeout = -1 if x_wait == TraLockResolution.WAIT else 0)
                tx_free_seeker = con_free_seeker.transaction_manager(custom_tpb)
                cur_free_seeker = tx_free_seeker.cursor()
                for test_dml in dml_list:
                    tx_free_seeker.begin()
                    try:
                        print('\n')
                        print(f'TIL = {x_isol.name}, WAIT = {x_wait.name}: {test_dml}')
                        cur_free_seeker.execute(f'{test_dml}')
                        c2col=cur_free_seeker.description
                        for r in cur_free_seeker:
                            for i in range(0,len(c2col)):
                                print( (c2col[i][0] +':').ljust(64), r[i] )

                    except DatabaseError as e:
                        print(e.__str__())
                        print(e.sqlcode)
                        for g in e.gds_codes:
                            print(g)
                    finally:
                        tx_free_seeker.rollback()

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
