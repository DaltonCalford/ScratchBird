#coding:utf-8

"""
ID:          issue-6987
ISSUE:       6987
TITLE:       DATEDIFF does not support fractional value for MILLISECOND
DESCRIPTION:
FBTEST:      bugs.core_6987
NOTES:
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

    select datediff(millisecond from timestamp '0001-01-01' to timestamp '0001-01-01 00:00:00.0001') dd_01 from rdb$database;
    select datediff(millisecond from timestamp '9999-12-31 23:59:59.9999' to timestamp '0001-01-01 00:00:00.0001') dd_02 from rdb$database;

    select datediff(millisecond from time '00:00:00' to time '00:00:00.0001') dd_03 from rdb$database;
    select datediff(millisecond from time '23:59:59' to time '00:00:00.0001') dd_04 from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('^((?!SQLSTATE|sqltype:|DD_).)*$', ''), ('[ \t]+', ' '), ('.*alias:.*', '')])

expected_stdout = """
    01: sqltype: 580 INT64 scale: -1 subtype: 0 len: 8
    : name: DATEDIFF alias: DD_01
    : table: owner:
    DD_01 0.1

    01: sqltype: 580 INT64 scale: -1 subtype: 0 len: 8
    : name: DATEDIFF alias: DD_02

    DD_02 -315537897599999.8

    01: sqltype: 580 INT64 scale: -1 subtype: 0 len: 8
    : name: DATEDIFF alias: DD_03
    DD_03 0.1

    01: sqltype: 580 INT64 scale: -1 subtype: 0 len: 8
    : name: DATEDIFF alias: DD_04
    DD_04 -86398999.9
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
