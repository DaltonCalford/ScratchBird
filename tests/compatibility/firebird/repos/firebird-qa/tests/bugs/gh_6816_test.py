#coding:utf-8

"""
ID:          issue-6816
ISSUE:       6816
TITLE:       Illegal output length in base64/hex_encode/decode functions
DESCRIPTION:
FBTEST:      bugs.gh_6816
NOTES:
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
    [04.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.894; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display on;
    set list on;
    select hex_encode(cast('' as varbinary(5))), base64_encode(cast('' as varbinary(5))) from rdb$database where 1 <> 1;
    -- produces lengths 14 & 12 with 10 & 8 expected
    select base64_decode(cast('' as varchar(4) character set utf8)), hex_decode(cast('' as varchar(4) character set utf8)) from rdb$database where 1<>1;
    -- produces lengths 12 & 8 with 3 & 2 expected
"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[ \t]+', ' ')])

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'SYSTEM.'
    expected_stdout = f"""
        01: sqltype: 448 VARYING scale: 0 subtype: 0 len: 10 charset: 2 {SQL_SCHEMA_PREFIX}ASCII
        02: sqltype: 448 VARYING scale: 0 subtype: 0 len: 8 charset: 2 {SQL_SCHEMA_PREFIX}ASCII
        01: sqltype: 448 VARYING scale: 0 subtype: 0 len: 3 charset: 1 {SQL_SCHEMA_PREFIX}OCTETS
        02: sqltype: 448 VARYING scale: 0 subtype: 0 len: 2 charset: 1 {SQL_SCHEMA_PREFIX}OCTETS
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
