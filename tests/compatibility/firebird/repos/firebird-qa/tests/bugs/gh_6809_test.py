#coding:utf-8

"""
ID:          issue-6809
ISSUE:       6809
TITLE:       Integer hex-literals: support for INT128
DESCRIPTION:
  NOTE that currently multiplication leads to following:
    int64 * int64 => int128
    int64 * int32 => int64
  In some cases one need to make explicit CAST of result to int128.
  For example, following attempts to evaluate result of multiplication -1 * (-9223372036854775808)
  will fail with integer overflow: -0x8000000000000000, -(0x8000000000000000); -1*(0x8000000000000000).
  Attempt to apply unary minus to -2^127 leads to "SQLSTATE = 22018 / conversion error from string".

  Discussed with Alex and dimitr 17-18 may 2021.
NOTES:
[27.07.2021]
  Adjusted expected* sections to results in current snapshots FB 4.x and 5.x: this is needed since fix #6874
FBTEST:      bugs.gh_6809
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set sqlda_display on;
    --select 0xF0000000 as "0xF0000000" from rdb$database; -- 496 LONG len: 4

    -- "If the number of <hexit> is less or equal than 8, the data type is a signed INTEGER"
    -- 496 LONG len: 4:
    select  0x7FFFFFFF  as "hex +0x7FFFFFFF" from rdb$database;   --  2147483647
    select  0x80000000  as "hex +0x80000000" from rdb$database;   -- -2147483648

    -- ::: NB-1 :::
    -- expected: 580 INT64 scale: 0 subtype: 0 len: 8; value: +2147483648
    select -0x80000000  as "hex -0x80000000" from rdb$database;         -- -2147483648 - NEGATIVE number!
    select -(0x80000000)  as "hex -(0x80000000)" from rdb$database;     -- -2147483648 - NEGATIVE number!
    select -1*(0x80000000)  as "hex -1*(0x80000000)" from rdb$database; -- +2147483648 // OK

    ------------------
    -- "If the number of <hexit> is greater than 8, the constant data type is a signed BIGINT"
    -- 580 INT64 scale: 0 subtype: 0 len: 8
    select  0x07FFFFFFF as "hex +0x07FFFFFFF" from rdb$database;  --  2147483647
    select -0x07FFFFFFF as "hex -0x07FFFFFFF" from rdb$database;  -- -2147483647
    select  0x080000000 as "hex +0x080000000" from rdb$database;  --  2147483648
    select -0x080000000 as "hex -0x080000000" from rdb$database;  -- -2147483648

    select  0x100000000 as "hex +0x100000000" from rdb$database;  --  4294967296
    select -0x100000000 as "hex -0x100000000" from rdb$database;  -- -4294967296

    select  0x7FFFFFFFFFFFFFFF as "hex +0x7FFFFFFFFFFFFFFF" from rdb$database;  --  9223372036854775807
    select -0x7FFFFFFFFFFFFFFF as "hex -0x7FFFFFFFFFFFFFFF" from rdb$database;  -- -9223372036854775807
    select  0x8000000000000000 as "hex +0x8000000000000000" from rdb$database;  -- -9223372036854775808

    -- ::: NB-2 :::
    -- SQLSTATE = 22003 / Integer overflow.  The result of an integer operation caused the most significant bit of the result to carry.
    select -0x8000000000000000 as "hex -0x8000000000000000" from rdb$database; -- SQLSTATE = 22003
    select -(0x8000000000000000) as "hex -0x8000000000000000" from rdb$database; -- SQLSTATE = 22003
    select -1*(0x8000000000000000) as "hex -1*(0x8000000000000000)" from rdb$database; -- SQLSTATE = 22003

    select  0x8000000000000001 as "hex +0x8000000000000001" from rdb$database; -- -9223372036854775807
    select  0xFFFFFFFFFFFFFFFF as "hex +0xFFFFFFFFFFFFFFFF" from rdb$database; -- -1


    ------------------
    -- "literals of 9-16 hex-pairs -> INT128"
    -- 32752 INT128 scale: 0 subtype: 0 len: 16
    select  0x07FFFFFFFFFFFFFFF as "hex +0x07FFFFFFFFFFFFFFF" from rdb$database;  -- 9223372036854775807

    -- ::: NB-3 :::
    -- Statement failed, SQLSTATE = 22018 / conversion error from string
    -- rather than "32752 INT128 scale: 0 subtype: 0 len: 16; value: -9223372036854775807"
    select -0x07FFFFFFFFFFFFFFF as "hex -0x07FFFFFFFFFFFFFFF" from rdb$database; -- SQLSTATE = 22018
    select -(0x07FFFFFFFFFFFFFFF) as "hex -(0x07FFFFFFFFFFFFFFF)" from rdb$database; -- SQLSTATE = 22018
    select -1*(0x07FFFFFFFFFFFFFFF) as "hex -1*(0x07FFFFFFFFFFFFFFF)" from rdb$database; -- -9223372036854775807 // OK


    select  0x08000000000000000 as "hex +0x08000000000000000" from rdb$database;  -- 9223372036854775808

    select  0x7FFFFFFFFFFFFFFF0 as "hex +0x7FFFFFFFFFFFFFFF0" from rdb$database;  -- 147573952589676412912

    select 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF as "hex 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" from rdb$database; -- 170141183460469231731687303715884105727
    select 0x80000000000000000000000000000000 as "hex 0x80000000000000000000000000000000" from rdb$database; -- -170141183460469231731687303715884105728

    -- ::: NB-4 :::
    -- Statement failed, SQLSTATE = 22018 / conversion error from string
    -- rather than "32752 INT128 scale: 0 subtype: 0 len: 16; value: -170141183460469231731687303715884105727"
    select -0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF as "hex 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" from rdb$database;
    select -(0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF) as "hex -(0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF)" from rdb$database;
    select -1*(0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF) as "hex -1*(0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF)" from rdb$database; -- -170141183460469231731687303715884105727 // OK

    -- Statement failed, SQLSTATE = 22018
    -- conversion error from string
    select -0x80000000000000000000000000000000 as "hex 0x80000000000000000000000000000000" from rdb$database;

    -- ::: NB-5 :::
    -- Expected:
    -- actual: -170141183460469231731687303715884105727
    select 0x80000000000000000000000000000001 as "hex 0x80000000000000000000000000000001" from rdb$database;

    select 0x70000000000000000000000000000001 as "hex +0x70000000000000000000000000000001" from rdb$database; -- +191408831393027885698148216680369618943
    select 0x8FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF as "hex +0x8FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" from rdb$database; -- -191408831393027885698148216680369618943
    select 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF as "hex +0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" from rdb$database; -- ?? -1 ?? // 340282366920938463463374607431768211455

    -- ::: NB-6 :::
    -- "SQLSTATE = 42000 / Dynamic SQL Error / -Token unknown / -as"
    -- (because number of hex pairs exceeds 16):
    select 0x0FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF as "hex +0x0FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF" from rdb$database;
    select 0x0000000000000000000000000000000000 as "hex +0x0000000000000000000000000000000000" from rdb$database;
    --------------------


    select 0x8FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF + 0x70000000000000000000000000000001 as "hex sum_01" from rdb$database; -- 0

    select 0x7FFFFFFF - 0x70000000000000000000000000000001 as "hex sub_01" from rdb$database;

    --select 36000/-4000/300 d from rdb$database;
    select 0x8ca0 / -0xfa0 / 3 as "hex div_01" from rdb$database;

    select 0x688589CC0E9505E2F2FEE5580000000
           / 0x2
           / 0x3
           / 0x4
           / 0x5
           / 0x6
           / 0x7
           / 0x8
           / 0x9
           / 0xa
           / 0xb
           / 0xc
           / 0xd
           / 0xe
           / 0xf
           / 0x10
           / 0x11
           / 0x12
           / 0x13
           / 0x14
           / 0x15
           / 0x16
           / 0x17
           / 0x18
           / 0x19
           / 0x1a
           / 0x1b
           / 0x1c
           / 0x1d
           / 0x1e
           / 0x1f
           / 0x20
     as "hex 33!/32!"
     from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('(-)?Token unknown.*', 'Token unknown'),
                                                 ('^((?!(sqltype|hex)).)*$', ''),
                                                 ('[ \t]+', ' ')])

expected_stderr = """
    Statement failed, SQLSTATE = 22003
    Integer overflow.  The result of an integer operation caused the most significant bit of the result to carry.

    Statement failed, SQLSTATE = 22003
    Integer overflow.  The result of an integer operation caused the most significant bit of the result to carry.

    Statement failed, SQLSTATE = 22018
    conversion error from string "-0X07FFFFFFFFFFFFFFF"

    Statement failed, SQLSTATE = 22018
    conversion error from string "-0X07FFFFFFFFFFFFFFF"

    Statement failed, SQLSTATE = 22018
    conversion error from string "-0X7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"

    Statement failed, SQLSTATE = 22018
    conversion error from string "-0X7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"

    Statement failed, SQLSTATE = 22018
    conversion error from string "-0X80000000000000000000000000000000"

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 44
    -as

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 45
    -as
"""

# version: 4.0

expected_stdout_1 = """
    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
      :  name: CONSTANT  alias: hex +0x7FFFFFFF
      : table:   owner:

    hex +0x7FFFFFFF                 2147483647



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
      :  name: CONSTANT  alias: hex +0x80000000
      : table:   owner:

    hex +0x80000000                 -2147483648



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
      :  name: CONSTANT  alias: hex -0x80000000
      : table:   owner:

    hex -0x80000000                 -2147483648



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
      :  name: CONSTANT  alias: hex -(0x80000000)
      : table:   owner:

    hex -(0x80000000)               -2147483648



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: MULTIPLY  alias: hex -1*(0x80000000)
      : table:   owner:

    hex -1*(0x80000000)             2147483648



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex +0x07FFFFFFF
      : table:   owner:

    hex +0x07FFFFFFF                2147483647



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex -0x07FFFFFFF
      : table:   owner:

    hex -0x07FFFFFFF                -2147483647



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex +0x080000000
      : table:   owner:

    hex +0x080000000                2147483648



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex -0x080000000
      : table:   owner:

    hex -0x080000000                -2147483648



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex +0x100000000
      : table:   owner:

    hex +0x100000000                4294967296



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex -0x100000000
      : table:   owner:

    hex -0x100000000                -4294967296



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex +0x7FFFFFFFFFFFFFFF
      : table:   owner:

    hex +0x7FFFFFFFFFFFFFFF         9223372036854775807



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex -0x7FFFFFFFFFFFFFFF
      : table:   owner:

    hex -0x7FFFFFFFFFFFFFFF         -9223372036854775807



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex +0x8000000000000000
      : table:   owner:

    hex +0x8000000000000000         -9223372036854775808



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex -0x8000000000000000
      : table:   owner:


    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex -0x8000000000000000
      : table:   owner:


    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: MULTIPLY  alias: hex -1*(0x8000000000000000)
      : table:   owner:

    hex -1*(0x8000000000000000)                               9223372036854775808



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex +0x8000000000000001
      : table:   owner:

    hex +0x8000000000000001         -9223372036854775807



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
      :  name: CONSTANT  alias: hex +0xFFFFFFFFFFFFFFFF
      : table:   owner:

    hex +0xFFFFFFFFFFFFFFFF         -1



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: CONSTANT  alias: hex +0x07FFFFFFFFFFFFFFF
      : table:   owner:

    hex +0x07FFFFFFFFFFFFFFF                                  9223372036854775807



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: MULTIPLY  alias: hex -1*(0x07FFFFFFFFFFFFFFF)
      : table:   owner:

    hex -1*(0x07FFFFFFFFFFFFFFF)                             -9223372036854775807



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: CONSTANT  alias: hex +0x08000000000000000
      : table:   owner:

    hex +0x08000000000000000                                  9223372036854775808



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: CONSTANT  alias: hex +0x7FFFFFFFFFFFFFFF0
      : table:   owner:

    hex +0x7FFFFFFFFFFFFFFF0                                147573952589676412912



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: CONSTANT  alias: hex 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
      : table:   owner:

    hex 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF       170141183460469231731687303715884105727



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: CONSTANT  alias: hex 0x80000000000000000000000000000000
      : table:   owner:

    hex 0x80000000000000000000000000000000      -170141183460469231731687303715884105728



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: MULTIPLY  alias: hex -1*(0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF)
      : table:   owner:

    hex -1*(0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF)      -170141183460469231731687303715884105727



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: CONSTANT  alias: hex 0x80000000000000000000000000000001
      : table:   owner:

    hex 0x80000000000000000000000000000001      -170141183460469231731687303715884105727



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: CONSTANT  alias: hex +0x70000000000000000000000000000001
      : table:   owner:

    hex +0x70000000000000000000000000000001       148873535527910577765226390751398592513



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: CONSTANT  alias: hex +0x8FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
      : table:   owner:

    hex +0x8FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF      -148873535527910577765226390751398592513



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: CONSTANT  alias: hex +0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
      : table:   owner:

    hex +0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF                                            -1



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: ADD  alias: hex sum_01
      : table:   owner:

    hex sum_01                                                                  0



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: SUBTRACT  alias: hex sub_01
      : table:   owner:

    hex sub_01                           -148873535527910577765226390749251108866



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: DIVIDE  alias: hex div_01
      : table:   owner:

    hex div_01                                                                 -3



    INPUT message field count: 0

    OUTPUT message field count: 1
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
      :  name: DIVIDE  alias: hex 33!/32!
      : table:   owner:

    hex 33!/32!                                                                33

"""

@pytest.mark.version('>=4.0,<5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_1
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

# version: 5.0

expected_stdout_2 = """
		01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
		: name: CONSTANT alias: hex +0x7FFFFFFF
		hex +0x7FFFFFFF 2147483647
		01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
		: name: CONSTANT alias: hex +0x80000000
		hex +0x80000000 -2147483648
		01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
		: name: CONSTANT alias: hex -0x80000000
		hex -0x80000000 -2147483648
		01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
		: name: CONSTANT alias: hex -(0x80000000)
		hex -(0x80000000) -2147483648
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: MULTIPLY alias: hex -1*(0x80000000)
		hex -1*(0x80000000) 2147483648
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex +0x07FFFFFFF
		hex +0x07FFFFFFF 2147483647
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex -0x07FFFFFFF
		hex -0x07FFFFFFF -2147483647
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex +0x080000000
		hex +0x080000000 2147483648
		01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
		: name: CONSTANT alias: hex -0x080000000
		hex -0x080000000 -2147483648
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex +0x100000000
		hex +0x100000000 4294967296
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex -0x100000000
		hex -0x100000000 -4294967296
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex +0x7FFFFFFFFFFFFFFF
		hex +0x7FFFFFFFFFFFFFFF 9223372036854775807
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex -0x7FFFFFFFFFFFFFFF
		hex -0x7FFFFFFFFFFFFFFF -9223372036854775807
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex +0x8000000000000000
		hex +0x8000000000000000 -9223372036854775808
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex -0x8000000000000000
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex -0x8000000000000000
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: MULTIPLY alias: hex -1*(0x8000000000000000)
		hex -1*(0x8000000000000000) 9223372036854775808
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex +0x8000000000000001
		hex +0x8000000000000001 -9223372036854775807
		01: sqltype: 580 INT64 scale: 0 subtype: 0 len: 8
		: name: CONSTANT alias: hex +0xFFFFFFFFFFFFFFFF
		hex +0xFFFFFFFFFFFFFFFF -1
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: CONSTANT alias: hex +0x07FFFFFFFFFFFFFFF
		hex +0x07FFFFFFFFFFFFFFF 9223372036854775807
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: MULTIPLY alias: hex -1*(0x07FFFFFFFFFFFFFFF)
		hex -1*(0x07FFFFFFFFFFFFFFF) -9223372036854775807
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: CONSTANT alias: hex +0x08000000000000000
		hex +0x08000000000000000 9223372036854775808
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: CONSTANT alias: hex +0x7FFFFFFFFFFFFFFF0
		hex +0x7FFFFFFFFFFFFFFF0 147573952589676412912
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: CONSTANT alias: hex 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
		hex 0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF 170141183460469231731687303715884105727
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: CONSTANT alias: hex 0x80000000000000000000000000000000
		hex 0x80000000000000000000000000000000 -170141183460469231731687303715884105728
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: MULTIPLY alias: hex -1*(0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF)
		hex -1*(0x7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF) -170141183460469231731687303715884105727
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: CONSTANT alias: hex 0x80000000000000000000000000000001
		hex 0x80000000000000000000000000000001 -170141183460469231731687303715884105727
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: CONSTANT alias: hex +0x70000000000000000000000000000001
		hex +0x70000000000000000000000000000001 148873535527910577765226390751398592513
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: CONSTANT alias: hex +0x8FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
		hex +0x8FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -148873535527910577765226390751398592513
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: CONSTANT alias: hex +0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
		hex +0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -1
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: ADD alias: hex sum_01
		hex sum_01 0
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: SUBTRACT alias: hex sub_01
		hex sub_01 -148873535527910577765226390749251108866
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: DIVIDE alias: hex div_01
		hex div_01 -3
		01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
		: name: DIVIDE alias: hex 33!/32!
		hex 33!/32! 33
"""

@pytest.mark.version('>=5.0')
def test_2(act: Action):
    act.expected_stdout = expected_stdout_2
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)
