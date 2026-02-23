#coding:utf-8

"""
ID:          issue-6636
ISSUE:       6636
TITLE:       Error converting string with hex representation of integer to smallint
DESCRIPTION:
  Bug initially was detected when adapted some of GTCS-tests which tries to convert
  numeric values from all possible combinations of datatypes.
  Particularly, conversion error was when try to cast "0x7fffffffffffffff" to bigint.
  See letter to Alex et al, 08-JUN-2020 15:38.

  It was decided to check here not only cast to smallint but also to other integer dadatypes.
JIRA:        CORE-6398
FBTEST:      bugs.core_6398
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter procedure "blob_smallint_explicit_cast" ( a blob ) returns ( b smallint ) as
    begin
        b = cast(a as smallint);
        suspend;
    end
    ^
    create or alter procedure "blob_int_explicit_cast" ( a blob ) returns ( b int ) as
    begin
        b = cast(a as int);
        suspend;
    end
    ^
    create or alter procedure "blob_bigint_explicit_cast" ( a blob ) returns ( b bigint ) as
    begin
        b = cast(a as bigint);
        suspend;
    end
    ^
    create or alter procedure "blob_int128_explicit_cast" ( a blob ) returns ( b int128 ) as
    begin
        b = cast(a as int128);
        suspend;
    end
    ^
    set term ;^
    commit;

    set heading off;

    select p.b as "blob_smallint_explicit_cast" from "blob_smallint_explicit_cast"('0x7fff') p;

    -- FAILED before fix: "numeric value is out of range":
    select p.b as "blob_smallint_explicit_cast" from "blob_smallint_explicit_cast"('0x8000') p;

    select p.b as "blob_int_explicit_cast" from "blob_int_explicit_cast"('0x7fffffff') p;
    select p.b as "blob_int_explicit_cast" from "blob_int_explicit_cast"('0x80000000') p;

    select p.b as "blob_bigint_explicit_cast" from "blob_bigint_explicit_cast"('0x7fffffffffffffff') p;
    select p.b as "blob_bigint_explicit_cast" from "blob_bigint_explicit_cast"('0x8000000000000000') p;

    select p.b as "blob_int128_explicit_cast" from "blob_int128_explicit_cast"('0x7fffffffffffffffffffffffffffffff') p;
    select p.b as "blob_int128_explicit_cast" from "blob_int128_explicit_cast"('0x80000000000000000000000000000000') p;
"""

act = isql_act('db', test_script)

expected_stdout = """
    32767
    -32768
    2147483647
    -2147483648
    9223372036854775807
    -9223372036854775808
    170141183460469231731687303715884105727
    -170141183460469231731687303715884105728
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
