#coding:utf-8

"""
ID:          issue-6409
ISSUE:       6409
TITLE:       SUBSTRING of non-text/-blob is described to return NONE character set in DSQL
DESCRIPTION:
  Confirmed output of: ' ... charset: 0 NONE' on 4.0.0.1627.
  Works as described in the ticket since 4.0.0.1632 ('... charset: 2 ASCII').
  NOTE. In the 'substitution' section we remove all rows except line with phrase 'charset' in it.
  Furter, we have to remove digital ID for this charset because it can be changed in the future:
  'charset: 2 ASCII' --> 'charset: ASCII'
JIRA:        CORE-6160
FBTEST:      bugs.core_6160
NOTES:
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
    [03.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- 01: sqltype: 448 VARYING scale: 0 subtype: 0 len: 1 charset: 2 ASCII
    set sqlda_display on;
    set planonly;
    select substring(1 from 1 for 1) from rdb$database;
    select substring(current_date from 1 for 1) from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|charset).)*$', ''), ('[ \t]+', ' '),
                                                 ('.*charset: [\\d]+', 'charset:')])
@pytest.mark.version('>=4.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else 'SYSTEM.'
    expected_stdout = f"""
        charset: {SQL_SCHEMA_PREFIX}ASCII
        charset: {SQL_SCHEMA_PREFIX}ASCII
    """


    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
