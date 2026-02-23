#coding:utf-8

"""
ID:          issue-6812
ISSUE:       6812
TITLE:       BASE64_ENCODE and HEX_ENCODE can exceed maximum widths for VARCHAR
DESCRIPTION:
FBTEST:      bugs.gh_6812
NOTES:
    [14.12.2023] pzotov
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
    select hex_encode(cast('' as char(32767))) as "enc_01" from rdb$database where 1 <> 1;
    select base64_encode(cast('' as char(32767))) as "enc_02" from rdb$database where 1 <> 1;
"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|sqltype|enc)).)*$', ''), ('[ \t]+', ' ')])

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'SYSTEM.'
    expected_stdout = f"""
        01: sqltype: 520 BLOB scale: 0 subtype: 1 len: 8 charset: 2 {SQL_SCHEMA_PREFIX}ASCII
        :  name: HEX_ENCODE  alias: enc_01
        01: sqltype: 520 BLOB scale: 0 subtype: 1 len: 8 charset: 2 {SQL_SCHEMA_PREFIX}ASCII
        :  name: BASE64_ENCODE  alias: enc_02
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
