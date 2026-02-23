#coding:utf-8

"""
ID:          issue-4306
ISSUE:       4306
TITLE:       Original table name and column name and owner missing from SQLDA for aliased column in grouped query
DESCRIPTION:
JIRA:        CORE-3973
FBTEST:      bugs.core_3973
NOTES:
    [11.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
    [28.06.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set planonly;
    set sqlda_display on;
    select rdb$relation_id as r_id, rdb$character_set_name
    from rdb$database
    group by rdb$relation_id, rdb$character_set_name;
"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|name|table)).)*$', ''), ('[ \t]+', ' ')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    expected_stdout_5x = f"""
        :  name: RDB$RELATION_ID  alias: R_ID
        : table: RDB$DATABASE  owner: {act.db.user.upper()}
        :  name: RDB$CHARACTER_SET_NAME  alias: RDB$CHARACTER_SET_NAME
        : table: RDB$DATABASE  owner: {act.db.user.upper()}
    """

    expected_stdout_6x = f"""
        : name: RDB$RELATION_ID alias: R_ID
        : table: RDB$DATABASE schema: SYSTEM owner: {act.db.user.upper()}
        : name: RDB$CHARACTER_SET_NAME alias: RDB$CHARACTER_SET_NAME
        : table: RDB$DATABASE schema: SYSTEM owner: {act.db.user.upper()}
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
