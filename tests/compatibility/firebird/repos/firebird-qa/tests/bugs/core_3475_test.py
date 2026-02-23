#coding:utf-8

"""
ID:          issue-3836
ISSUE:       3836
TITLE:       Parameters inside the CAST function are described as not nullable
DESCRIPTION:
JIRA:        CORE-3475
FBTEST:      bugs.core_3475
NOTES:
    [11.12.2023] pzotov
    Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set planonly;
    set sqlda_display;
    select cast(null as int) v1, cast(? as int) v2 from rdb$database;
"""

act = isql_act('db', test_script, substitutions = [ ('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[ \t]+', ' ') ] )

expected_stdout = """
    01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    02: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

