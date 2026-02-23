#coding:utf-8

"""
ID:          issue-8108
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8108
TITLE:       Engine returns empty string when unable to translate Unicode symbol into ICU-codepage.
DESCRIPTION:
NOTES:
    [15.05.2024] pzotov
    Confirmed ticket notes on 4.0.5.3092: empty string is returned instead of error with SQLSTATE = 22018.
    Checked on intermediate snapshots: 6.0.0.351 #02ef0c8, 5.0.1.1399 #5b8b57c, 4.0.5.3099 #bc1ad78
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    -- Hangul Choseong Filler
    -- x115F;
    -- FB3 returns an error, FB4 OK (an error is expected)
    select '>' || cast(_utf8 'ᅟ' as varchar(1) character set tis620) || '<'  from rdb$database;
    --select '>' || cast( cast(unicode_char(0x115f) as varchar(1) character set utf8) as varchar(1) character set tis620) || '<' from rdb$database;

    -- FB3 and FB4 return an error (it is OK)
    select '>' || cast(_utf8 'ᅟ' as varchar(1) character set win1251) || '<'  from rdb$database;
    --select '>' || cast( cast(unicode_char(0x115f) as varchar(1) character set utf8) as varchar(1) character set win1251) || '<' from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    Statement failed, SQLSTATE = 22018
    arithmetic exception, numeric overflow, or string truncation
    -Cannot transliterate character between character sets

    Statement failed, SQLSTATE = 22018
    arithmetic exception, numeric overflow, or string truncation
    -Cannot transliterate character between character sets
"""

@pytest.mark.version('>=4.0.5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
