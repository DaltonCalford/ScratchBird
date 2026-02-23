#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8842
TITLE:       GTT accept weird syntax and has unnecessary syntax conflicts.
DESCRIPTION:
NOTES:
    [04.01.2026] pzotov
    Confirmed bug on 6.0.0.1377 (error message not appears).
    Checked on 6.0.0.1387.
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    create global temporary table gtt (x integer) ,,,,;
"""

substitutions = [('(-)?SQL error code.*', ''), (r'(-)?Token unknown.* line(:)?\s+\d+.*', 'Token unknown')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -Token unknown - line 11, column 417
    -,
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
