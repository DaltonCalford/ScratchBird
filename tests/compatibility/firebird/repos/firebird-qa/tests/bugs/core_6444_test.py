#coding:utf-8

"""
ID:          issue-6677
ISSUE:       6677
TITLE:       Ability to query Firebird configuration using SQL
DESCRIPTION:
JIRA:        CORE-6444
FBTEST:      bugs.core_6444
NOTES:
    [01.12.2023] pzotov
        Currently test only checks ability to query virtual table RDB$CONFIG and SQLDA.
        Records are not fetched because content of some of them depends on OS/major version and/or can change.
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
    [03.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.892; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813
"""
import os
import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set planonly;
    set sqlda_display on;
    select * from rdb$config;
"""
act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|sqltype:|name:).)*$',''),('[ \t]+',' ')])

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  'SYSTEM.'
    act.expected_stdout = f"""
        01: sqltype: 496 LONG scale: 0 subtype: 0 len: 4
        :  name: RDB$CONFIG_ID  alias: RDB$CONFIG_ID
        02: sqltype: 448 VARYING scale: 0 subtype: 0 len: 63 charset: 2 {SQL_SCHEMA_PREFIX}ASCII
        :  name: RDB$CONFIG_NAME  alias: RDB$CONFIG_NAME
        03: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 1020 charset: 4 {SQL_SCHEMA_PREFIX}UTF8
        :  name: RDB$CONFIG_VALUE  alias: RDB$CONFIG_VALUE
        04: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 1020 charset: 4 {SQL_SCHEMA_PREFIX}UTF8
        :  name: RDB$CONFIG_DEFAULT  alias: RDB$CONFIG_DEFAULT
        05: sqltype: 32764 BOOLEAN scale: 0 subtype: 0 len: 1
        :  name: RDB$CONFIG_IS_SET  alias: RDB$CONFIG_IS_SET
        06: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 1020 charset: 4 {SQL_SCHEMA_PREFIX}UTF8
        :  name: RDB$CONFIG_SOURCE  alias: RDB$CONFIG_SOURCE
    """
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
