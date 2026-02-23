#coding:utf-8

"""
ID:          issue-6542
ISSUE:       6542
TITLE:       Next attachment id, next statement id and some other additions [CORE-6300]
DESCRIPTION:
    Check SQLDA output by query mon$database columns and context variabled that are described
    in doc/sql.extensions/README.context_variables2
    See also: https://github.com/FirebirdSQL/firebird/commit/22ad236f625716f5f2885f8d9e783cca9516f7b3
JIRA:        CORE-6300
FBTEST:      bugs.core_6300
NOTES:
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.

    [03.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214.

"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display on;
    set planonly;
    select mon$guid,mon$file_id,mon$next_attachment,mon$next_statement, rdb$get_context('SYSTEM', 'DB_GUID'), rdb$get_context('SYSTEM', 'DB_FILE_ID')
    from mon$database;
"""

act = isql_act('db', test_script, substitutions = [ ( '^((?!SQLSTATE|sqltype|name:).)*$', ''), ('[ \t]+', ' ') ] )

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  'SYSTEM.'
    expected_stdout = f"""
        01: sqltype: 452 TEXT Nullable scale: 0 subtype: 0 len: 38 charset: 0 {SQL_SCHEMA_PREFIX}NONE
        :  name: MON$GUID  alias: MON$GUID
        02: sqltype: 448 VARYING scale: 0 subtype: 0 len: 255 charset: 2 {SQL_SCHEMA_PREFIX}ASCII
        :  name: MON$FILE_ID  alias: MON$FILE_ID
        03: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        :  name: MON$NEXT_ATTACHMENT  alias: MON$NEXT_ATTACHMENT
        04: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
        :  name: MON$NEXT_STATEMENT  alias: MON$NEXT_STATEMENT
        05: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 255 charset: 0 {SQL_SCHEMA_PREFIX}NONE
        :  name: RDB$GET_CONTEXT  alias: RDB$GET_CONTEXT
        06: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 255 charset: 0 {SQL_SCHEMA_PREFIX}NONE
        :  name: RDB$GET_CONTEXT  alias: RDB$GET_CONTEXT
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
