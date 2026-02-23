#coding:utf-8

"""
ID:          issue-6874
ISSUE:       6874
TITLE:       Literal 65536 (interpreted as int) can not be multiplied by itself w/o cast if result more than 2^63-1
DESCRIPTION:
  Confirmed need to explicitly cast literal 65536 on: 5.0.0.88, 4.0.1.2523 (otherwise get SQLSTATE = 22003).
FBTEST:      bugs.gh_6874
NOTES:
    [14.12.2023] pzotov
        Added 'SQLSTATE' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
        ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
        Added 'combine_output = True' in order to see SQLSTATE if any error occurs.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set sqlda_display on;
    select 65536*65536*65536*65536 as "multiply_result_1" from rdb$database;
    select -65536*-65536*-65536*-65536 as "multiply_result_2" from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|sqltype:|multiply_result).)*$', ''), ('[ \t]+', ' '), ('.*alias:.*', '')])

expected_stdout = """
    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
    multiply_result_1                18446744073709551616

    01: sqltype: 32752 INT128 scale: 0 subtype: 0 len: 16
    multiply_result_2                18446744073709551616
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
