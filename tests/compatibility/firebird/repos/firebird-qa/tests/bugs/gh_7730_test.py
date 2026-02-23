#coding:utf-8

"""
ID:          issue-7730
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7730
TITLE:       Server ignores the size of VARCHAR when performing SET BIND ... TO VARCHAR(N)
DESCRIPTION:
NOTES:
    [25.08.2023] pzotov
        Confirmed problem on 5.0.0.1169, 4.0.4.2982
        Checked on 5.0.0.1177, 4.0.4.2982 (intermediate snapshots).
    [14.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
    [05.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214.
"""

import locale
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions = [ ('^((?!SQLSTATE|sqltype:).)*$',''),('[ \t]+',' ' ) ] )

CHK_TIMESTAMP = '2023-08-29 01:02:03.0123 +03:00'
test_sql = f"""
    SET BIND OF TIMESTAMP WITH TIME ZONE TO varchar(128);
    set sqlda_display on;
    set planonly;
    select timestamp '{CHK_TIMESTAMP}' from rdb$database;
"""


@pytest.mark.version('>=4.0.4')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'SYSTEM.'
    expected_stdout = f"""
        01: sqltype: 448 VARYING scale: 0 subtype: 0 len: 128 charset: 0 {SQL_SCHEMA_PREFIX}NONE
    """
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_sql, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
