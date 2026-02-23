#coding:utf-8

"""
ID:          decfloat.parsing-scaled-integers-and-bigint-max-min
TITLE:       Interpretation of DECFLOAT values as BIGINT
DESCRIPTION:
    Check commit: "Fixed parsing of scaled integers and MAX/MIN INT64", 2017-05-28
    See: github.com/FirebirdSQL/firebird/commit/1278d0692b535f69c7f9e208aad9682980ed9c59
FBTEST:      functional.datatypes.decfloat_parsing_scaled_integers_and_bigint_max_min
NOTES:
    [10.12.2019]
        Updated syntax for SET BIND command because it was changed in 11-nov-2019.
    [30.12.2019]
        Updated code and expected_stdout - get it from Alex, see letter 30.12.2019 16:15.
    [25.06.2020]
        Changed types in SQLDA from numeric to int128 // after discuss with Alex about CORE-6342.
    [01.07.2020]
        Adjusted expected output ('subtype' values).
        Removed unnecessary lines from output and added substitution section for result to be properly filtered.

        Found a problem with interpreting values
        170141183460469231731687303715884105727 and -170141183460469231731687303715884105728
        Sent letter to Alex (01.07.2020 13:55), waiting for fix. Check of bind DECFLOAT to INT128 was deferred.
    [16.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
     set list on;
     set sqlda_display on;

     recreate view v_test as
     select
        -9223372036854775809 as behind_bigint_min
        ,9223372036854775808 as behind_bigint_max
        ,-9223372036854775808 as bigint_min
        ,9223372036854775807 as bigint_max
        ,0.0123456789123456789 as drob1
     from rdb$database;

     select * from  v_test;

     set bind of bigint to double precision;
     set sqlda_display off;

     select * from  v_test;
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|sqltype|BEHIND_BIGINT_|BIGINT_|DROB1).)*$', ''),
                                                 ('[ \t]+', ' ')])

expected_stdout = """
    01: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
    :  name: BEHIND_BIGINT_MIN  alias: BEHIND_BIGINT_MIN
    02: sqltype: 32752 INT128 Nullable scale: 0 subtype: 0 len: 16
    :  name: BEHIND_BIGINT_MAX  alias: BEHIND_BIGINT_MAX
    03: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    :  name: BIGINT_MIN  alias: BIGINT_MIN
    04: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    :  name: BIGINT_MAX  alias: BIGINT_MAX
    05: sqltype: 580 INT64 Nullable scale: -19 subtype: 0 len: 8
    :  name: DROB1  alias: DROB1
    BEHIND_BIGINT_MIN                                        -9223372036854775809
    BEHIND_BIGINT_MAX                                         9223372036854775808
    BIGINT_MIN                      -9223372036854775808
    BIGINT_MAX                      9223372036854775807
    DROB1                           0.0123456789123456789
    BEHIND_BIGINT_MIN                                        -9223372036854775809
    BEHIND_BIGINT_MAX                                         9223372036854775808
    BIGINT_MIN                      -9.223372036854776e+18
    BIGINT_MAX                      9.223372036854776e+18
    DROB1                           0.01234567891234568
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
