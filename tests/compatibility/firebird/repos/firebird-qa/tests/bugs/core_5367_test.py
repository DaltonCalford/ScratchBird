#coding:utf-8

"""
ID:          issue-5640
ISSUE:       5640
TITLE:       Regression: (boolean) parameters as search condition no longer allowed
DESCRIPTION:
JIRA:        CORE-5367
FBTEST:      bugs.core_5367
NOTES:
    [01.07.2025] pzotov
    Refactored: we have to check only rows which contain either 'sqltype' or 'SQLSTATE'.
    Added appropriate substitutions.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(id int, boo boolean);
    set sqlda_display on;
    set planonly;
    select * from test where ?;
"""


substitutions=[('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    01: sqltype: 32764 BOOLEAN scale: 0 subtype: 0 len: 1
    01: sqltype: 496 LONG Nullable scale: 0 subtype: 0 len: 4
    02: sqltype: 32764 BOOLEAN Nullable scale: 0 subtype: 0 len: 1
"""

@pytest.mark.version('>=3.0.2')
def test_1(act: Action):

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
