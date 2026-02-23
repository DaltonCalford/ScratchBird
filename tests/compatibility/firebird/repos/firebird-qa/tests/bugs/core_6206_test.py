#coding:utf-8

"""
ID:          issue-6451
ISSUE:       6451
TITLE:       VARCHAR of insufficient length used for set bind of decfloat to varchar
DESCRIPTION:
NOTES:
[26.06.2020]
  changed SET BIND argument from numeric(38) to INT128, adjusted output
  (letter from Alex, 25.06.2020 17:56; needed after discuss CORE-6342).
JIRA:        CORE-6206
FBTEST:      bugs.core_6206
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display on;
    set list on;

    set bind of decfloat to char;
    select -1.234567890123456789012345678901234E+6144 as decfloat_to_char from rdb$database;

    set bind of decfloat to varchar;
    select -1.234567890123456789012345678901234E+6144 as decfloat_to_varchar from rdb$database;

    --set bind of numeric(38) to char;
    set bind of int128 to char;
    select 12345678901234567890123456789012345678 as n38_to_char from rdb$database;

    --set bind of numeric(38) to varchar;
    set bind of int128 to char;
    select 12345678901234567890123456789012345678 as n38_to_varchar from rdb$database;

"""

act = isql_act('db', test_script, substitutions=[('charset.*', ''), ('[ \t]+', ' '),
                                                 ('^((?!(sqltype)).)*$', '')])

expected_stdout = """
    01: sqltype: 452 TEXT scale: 0 subtype: 0 len: 42
    01: sqltype: 448 VARYING scale: 0 subtype: 0 len: 42
    01: sqltype: 452 TEXT scale: 0 subtype: 0 len: 47
    01: sqltype: 452 TEXT scale: 0 subtype: 0 len: 47
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
