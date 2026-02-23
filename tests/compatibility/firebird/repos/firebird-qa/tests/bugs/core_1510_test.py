#coding:utf-8

"""
ID:          issue-1925
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1925
TITLE:       Bad XSQLVAR [NULL flags] for (2*COALESCE(NULL,NULL))
DESCRIPTION:
JIRA:        CORE-1510
FBTEST:      bugs.core_1510
NOTES:
    [10.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set sqlda_display;
    select 2*coalesce(null,null) from rdb$database;
    select 2*iif(null is null, null, null) from rdb$database;
    -- NB! This must result NULL rather than zero division:
    select null/0 from rdb$database;
"""

substitutions = [ ('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[ ]+', ' '),
                  ('[ \t]+', ' '), ('charset:.*', '')
                ]
act = isql_act('db', test_script, substitutions = substitutions )

expected_stdout = """
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
