#coding:utf-8

"""
ID:          issue-6578
ISSUE:       6578
TITLE:       SubType information is lost when calculating arithmetic expressions
DESCRIPTION:
JIRA:        CORE-6337
FBTEST:      bugs.core_6337
NOTES:
    [25.06.2020]
        4.0.0.2076: changed types in SQLDA from numeric to int128 // after discuss with Alex about CORE-6342.
    [13.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display on;
    set list on;
    select cast(1 as numeric(18,2)) * cast(1 as numeric(18,2)) from rdb$database;
"""

act = isql_act('db', test_script, substitutions = [ ('^((?!SQLSTATE|sqltype).)*$', ''), ('[ \t]+', ' ') ] )

expected_stdout = """
    01: sqltype: 32752 INT128 scale: -4 subtype: 1 len: 16
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
