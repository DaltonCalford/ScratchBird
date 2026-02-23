#coding:utf-8

"""
ID:          issue-6797
ISSUE:       6797
TITLE:       Functions DECRYPT and RSA_DECRYPT return VARCHAR CHARACTER SET NONE instead of VARBINARY (VARCHAR) CHARACTER SET OCTETS
DESCRIPTION:
    As of current FB 4.x doc, following is wrong: "Functions return ... *varbinary* for all other types."
    (see note by Alex in the tracker, 11.05.2021 11:17).
FBTEST:      bugs.gh_6797
NOTES:
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
    [04.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.894; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set blob all;
    set list on;
    set sqlda_display on;
    set planonly;
    select
        decrypt(cast('' as varchar(1)) using aes mode ofb key '0123456701234567' iv '1234567890123456') as decrypt_vchr
       ,decrypt(cast('' as blob) using aes mode ofb key '0123456701234567' iv '1234567890123456') as decrypt_blob
    from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[ \t]+', ' ')])

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'SYSTEM.'
    expected_stdout = f"""
        01: sqltype: 448 VARYING scale: 0 subtype: 0 len: 1 charset: 1 {SQL_SCHEMA_PREFIX}OCTETS
        02: sqltype: 520 BLOB scale: 0 subtype: 0 len: 8
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
