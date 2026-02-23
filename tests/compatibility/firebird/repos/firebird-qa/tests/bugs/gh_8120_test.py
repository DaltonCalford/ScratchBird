#coding:utf-8

"""
ID:          issue-8120
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8120
TITLE:       Cast dies with numeric value is out of range error
DESCRIPTION:
NOTES:
    [27.05.2024] pzotov
    Confirmed bug on 4.0.5.3077.
    Checked on 6.0.0.362; 5.0.1.1408; 4.0.5.3103
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    select cast('27' as numeric(4,2)) from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    27.00
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
