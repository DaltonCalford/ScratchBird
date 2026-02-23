#coding:utf-8

"""
ID:          int128.binary-operations
ISSUE:       6583
JIRA:        CORE-6342
TITLE:       Basic test for binary functions against INT128 datatype
DESCRIPTION:
  Test verifies https://github.com/FirebirdSQL/firebird/commit/137c3a96e51b8bc34cb74732687067e96c971226
  (Postfix for #6583: enable support of int128 in bin_* family of functions).
FBTEST:      functional.datatypes.int128_binary_operations
NOTES:
    [20.06.2023] pzotov
    Evaluation of bin_shr(-2^127, 127) should give result of -1. Before 5.0.0.1082 it was wrong: +1.
    Checked on 5.0.0.1082.
    Fix for FB 4.x will be soon (message from Alex, 20.06.2023 08:42).
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    recreate table test_i128(bi_least int128, bi_great int128);
    commit;

    insert into test_i128(bi_least, bi_great) values(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727);


    -- All subsequent expressions before 4.0.0.2083 raised:
    -- Statement failed, SQLSTATE = 22003
    -- arithmetic exception, numeric overflow, or string truncation
    -- -Integer overflow.  The result of an integer operation caused the most significant bit of the result to carry.

    select bin_and(bi_least,bi_great) from test_i128; -- expected: 0
    select bin_not(bi_least) from test_i128; -- expected: 170141183460469231731687303715884105727
    select bin_not(bi_great) from test_i128; -- expected: -170141183460469231731687303715884105728
    select bin_or(bi_least, bi_great) from test_i128; -- expected: -1
    
    select bin_shr(bi_least,127) from test_i128;  -- proper result: -1; before 5.0.0.1082 was wrong: +1

    select bin_shr(bi_great,127) from test_i128;  -- expected: 0
    select bin_shl(bi_least,127) from test_i128; -- expected: 0
    select bin_and( bin_shl(bi_great,127), bi_least) from test_i128; -- expected: -170141183460469231731687303715884105728
    select bin_xor(bi_least, bi_great) from test_i128; -- expected: -1
    select bin_xor(bi_least, bi_great) from test_i128; -- expected: -1
    select bin_xor(bi_least, bin_xor(bi_least, bi_great)) from test_i128; -- expected: 170141183460469231731687303715884105727
"""

act = isql_act('db', test_script)

expected_stdout = """
    0
    170141183460469231731687303715884105727
    -170141183460469231731687303715884105728
    -1
    -1
    0
    0
    -170141183460469231731687303715884105728
    -1
    -1
    170141183460469231731687303715884105727
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
