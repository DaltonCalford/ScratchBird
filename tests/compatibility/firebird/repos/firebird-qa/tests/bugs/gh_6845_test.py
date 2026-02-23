#coding:utf-8

"""
ID:          issue-6845
ISSUE:       6845
TITLE:       Result type of AVG over BIGINT column results in type INT128
DESCRIPTION:
FBTEST:      bugs.gh_6845
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
    recreate table test(x bigint, y decfloat(16));

    set sqlda_display on;
    set list on;
    select avg(x) as avg_bigint, avg(y) as avg_decf16 from test having false;
    select avg(x)over() as avg_bigint_over, avg(y)over() as avg_decf16_over from test;
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|sqltype:|multiply_result).)*$', ''), ('[ \t]+', ' '), ('.*alias:.*', '')])

expected_stdout = """
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    02: sqltype: 32760 DECFLOAT(16) Nullable scale: 0 subtype: 0 len: 8

    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    02: sqltype: 32760 DECFLOAT(16) Nullable scale: 0 subtype: 0 len: 8
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
