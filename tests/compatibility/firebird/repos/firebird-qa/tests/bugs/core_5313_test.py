#coding:utf-8

"""
ID:          issue-5590
ISSUE:       5590
TITLE:       Data type unknown error with LIST
DESCRIPTION:
JIRA:        CORE-5313
FBTEST:      bugs.core_5313
NOTES:
    [01.07.2025] pzotov
    Refactored: we have to check only rows which contain either 'sqltype' or 'SQLSTATE'.
    Added appropriate substitutions.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set planonly;
    set sqlda_display on;
    select list(trim(rdb$relation_name), ?) from rdb$relations rows 0;
"""

substitutions=[('^((?!(SQLSTATE|sqltype)).)*$', ''), ('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_3x = """
    01: sqltype: 452 TEXT scale: 0 subtype: 0 len: 3 charset: 3 UNICODE_FSS
    01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 3 UNICODE_FSS
"""

expected_stdout_5x = """
    01: sqltype: 452 TEXT scale: 0 subtype: 0 len: 4 charset: 4 UTF8
    01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 UTF8
"""

expected_stdout_6x = """
    01: sqltype: 452 TEXT scale: 0 subtype: 0 len: 4 charset: 4 SYSTEM.UTF8
    01: sqltype: 520 BLOB Nullable scale: 0 subtype: 1 len: 8 charset: 4 SYSTEM.UTF8
"""


@pytest.mark.version('>=3.0')
def test_1(act: Action):

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
