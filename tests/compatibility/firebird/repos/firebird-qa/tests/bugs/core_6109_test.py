#coding:utf-8

"""
ID:          issue-6358
ISSUE:       6358
TITLE:       Changing FLOAT to a SQL standard compliant FLOAT datatype
DESCRIPTION:
JIRA:        CORE-6109
FBTEST:      bugs.core_6109
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
    recreate table test(
       r real
      ,f float
      ,f01 float( 1)
      ,f24 float(24)
      ,f25 float(25)
      ,f53 float(53)
    );

    set list on;
    set sqlda_display on;
    select * from test;
"""

act = isql_act('db', test_script, substitutions=[('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[ \t]+', ' ')])

expected_stdout = """
    01: sqltype: 482 FLOAT Nullable scale: 0 subtype: 0 len: 4
    02: sqltype: 482 FLOAT Nullable scale: 0 subtype: 0 len: 4
    03: sqltype: 482 FLOAT Nullable scale: 0 subtype: 0 len: 4
    04: sqltype: 482 FLOAT Nullable scale: 0 subtype: 0 len: 4
    05: sqltype: 480 DOUBLE Nullable scale: 0 subtype: 0 len: 8
    06: sqltype: 480 DOUBLE Nullable scale: 0 subtype: 0 len: 8
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
