#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/8795
TITLE:       GENERATE_SERIES function - advanced tests
DESCRIPTION:
NOTES:
    [02.01.2026] pzotov
    Checked on 6.0.0.1385
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set bail on;
    -- See letter to sim-mail@list.ru et al, 20.11.2025 2342 (found on 6.0.0.1357-d37c931):
    select * from generate_series(-9223372036854775807, -9223372036854775808, -1) as place_holder_name( generated_values_000 ) rows 10;

    -- See letter from sim-mail@list.ru, 26.11.2025 1128:
    select * from generate_series(9223372036854775806, 9223372036854775807, 1) as place_holder_name( generated_values_010 ) rows 10;
    select * from generate_series(9223372036854775806, 9223372036854775807, 2) as place_holder_name( generated_values_020 ) rows 10;
    select * from generate_series(9223372036854775806, 9223372036854775807, 9223372036854775807) as place_holder_name( generated_values_030 ) rows 10;
    select * from generate_series(-9223372036854775807, 9223372036854775807, 9223372036854775807) as place_holder_name( generated_values_040 ) rows 10;
    select * from generate_series(-9223372036854775807, -9223372036854775808, -1) as place_holder_name( generated_values_050 ) rows 10;
    select * from generate_series(-9223372036854775807, -9223372036854775808, -2) as place_holder_name( generated_values_060 ) rows 10;
    select * from generate_series(-9223372036854775807, -9223372036854775808, -9223372036854775808) as place_holder_name( generated_values_070 ) rows 10;
    select * from generate_series(9223372036854775807, -9223372036854775808, -1) as place_holder_name( generated_values_080 ) rows 10;
    select * from generate_series(9223372036854775807, -9223372036854775808, -9223372036854775808) as place_holder_name( generated_values_090 ) rows 10;

    select * from generate_series(
      bin_shl(cast(2 as int128),125)-2 + bin_shl(cast(2 as int128),125), 
      bin_shl(cast(2 as int128),125)-1 + bin_shl(cast(2 as int128),125), 
      1
    ) as place_holder_name( generated_values_100 ) rows 10;

    select * from generate_series(
      bin_shl(cast(2 as int128),125)-2 + bin_shl(cast(2 as int128),125), 
      bin_shl(cast(2 as int128),125)-1 + bin_shl(cast(2 as int128),125), 
      2
    ) as place_holder_name( generated_values_110 ) rows 10;
          
    select * from generate_series(
      bin_shl(cast(2 as int128),125)-2 + bin_shl(cast(2 as int128),125), 
      bin_shl(cast(2 as int128),125)-1 + bin_shl(cast(2 as int128),125), 
      bin_shl(cast(2 as int128),125)-1 + bin_shl(cast(2 as int128),125)
    ) as place_holder_name( generated_values_120 ) rows 10;      

    select * from generate_series(
      bin_shl(cast(2 as int128),126), 
      bin_shl(cast(2 as int128),125)-1 + bin_shl(cast(2 as int128),125), 
      1
    ) as place_holder_name( generated_values_130 ) rows 10;     

    select * from generate_series(
      bin_shl(cast(2 as int128),126), 
      bin_shl(cast(2 as int128),125)-1 + bin_shl(cast(2 as int128),125), 
      bin_shl(cast(2 as int128),125)-1 + bin_shl(cast(2 as int128),125)
    ) as place_holder_name( generated_values_140 ) rows 10;  

    select * from generate_series(
      bin_shl(cast(2 as int128),126)+1, 
      bin_shl(cast(2 as int128),126), 
      -1
    ) as place_holder_name( generated_values_150 ) rows 10;

    select * from generate_series(
      bin_shl(cast(2 as int128),126)+1, 
      bin_shl(cast(2 as int128),126), 
      -2
    ) as place_holder_name( generated_values_160 ) rows 10;

    select * from generate_series(
      bin_shl(cast(2 as int128),126)+1, 
      bin_shl(cast(2 as int128),126), 
      bin_shl(cast(2 as int128),126)
    ) as place_holder_name( generated_values_170 ) rows 10;     

    select * from generate_series(
      bin_shl(cast(2 as int128),125)-1 + bin_shl(cast(2 as int128),125), 
      bin_shl(cast(2 as int128),126), 
      bin_shl(cast(2 as int128),126)
    ) as place_holder_name( generated_values_180 ) rows 10;       

    select * from generate_series(
      bin_shl(cast(2 as int128),125)-1 + bin_shl(cast(2 as int128),125), 
      bin_shl(cast(2 as int128),126), 
      -1
    ) as place_holder_name( generated_values_190 ) rows 10; 
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        GENERATED_VALUES_000 -9223372036854775807
        GENERATED_VALUES_000 -9223372036854775808
        GENERATED_VALUES_010 9223372036854775806
        GENERATED_VALUES_010 9223372036854775807
        GENERATED_VALUES_020 9223372036854775806
        GENERATED_VALUES_030 9223372036854775806
        GENERATED_VALUES_040 -9223372036854775807
        GENERATED_VALUES_040 0
        GENERATED_VALUES_040 9223372036854775807
        GENERATED_VALUES_050 -9223372036854775807
        GENERATED_VALUES_050 -9223372036854775808
        GENERATED_VALUES_060 -9223372036854775807
        GENERATED_VALUES_070 -9223372036854775807
        GENERATED_VALUES_080 9223372036854775807
        GENERATED_VALUES_080 9223372036854775806
        GENERATED_VALUES_080 9223372036854775805
        GENERATED_VALUES_080 9223372036854775804
        GENERATED_VALUES_080 9223372036854775803
        GENERATED_VALUES_080 9223372036854775802
        GENERATED_VALUES_080 9223372036854775801
        GENERATED_VALUES_080 9223372036854775800
        GENERATED_VALUES_080 9223372036854775799
        GENERATED_VALUES_080 9223372036854775798
        GENERATED_VALUES_090 9223372036854775807
        GENERATED_VALUES_090 -1
        GENERATED_VALUES_100 170141183460469231731687303715884105726
        GENERATED_VALUES_100 170141183460469231731687303715884105727
        GENERATED_VALUES_110 170141183460469231731687303715884105726
        GENERATED_VALUES_120 170141183460469231731687303715884105726
        GENERATED_VALUES_130 -170141183460469231731687303715884105728
        GENERATED_VALUES_130 -170141183460469231731687303715884105727
        GENERATED_VALUES_130 -170141183460469231731687303715884105726
        GENERATED_VALUES_130 -170141183460469231731687303715884105725
        GENERATED_VALUES_130 -170141183460469231731687303715884105724
        GENERATED_VALUES_130 -170141183460469231731687303715884105723
        GENERATED_VALUES_130 -170141183460469231731687303715884105722
        GENERATED_VALUES_130 -170141183460469231731687303715884105721
        GENERATED_VALUES_130 -170141183460469231731687303715884105720
        GENERATED_VALUES_130 -170141183460469231731687303715884105719
        GENERATED_VALUES_140 -170141183460469231731687303715884105728
        GENERATED_VALUES_140 -1
        GENERATED_VALUES_140 170141183460469231731687303715884105726
        GENERATED_VALUES_150 -170141183460469231731687303715884105727
        GENERATED_VALUES_150 -170141183460469231731687303715884105728
        GENERATED_VALUES_160 -170141183460469231731687303715884105727
        GENERATED_VALUES_170 -170141183460469231731687303715884105727
        GENERATED_VALUES_180 170141183460469231731687303715884105727
        GENERATED_VALUES_180 -1
        GENERATED_VALUES_190 170141183460469231731687303715884105727
        GENERATED_VALUES_190 170141183460469231731687303715884105726
        GENERATED_VALUES_190 170141183460469231731687303715884105725
        GENERATED_VALUES_190 170141183460469231731687303715884105724
        GENERATED_VALUES_190 170141183460469231731687303715884105723
        GENERATED_VALUES_190 170141183460469231731687303715884105722
        GENERATED_VALUES_190 170141183460469231731687303715884105721
        GENERATED_VALUES_190 170141183460469231731687303715884105720
        GENERATED_VALUES_190 170141183460469231731687303715884105719
        GENERATED_VALUES_190 170141183460469231731687303715884105718
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
