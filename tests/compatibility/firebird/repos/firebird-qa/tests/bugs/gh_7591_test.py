#coding:utf-8

"""
ID:          issue-7591
ISSUE:       7591
TITLE:       RELEASE SAVEPOINT ONLY works incorrectly
DESCRIPTION:
    [24.05.2023] pzotov
    Confirmed bug on 4.0.3.2933.
    Checked on 4.0.3.2942 - all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    Completed.
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
    data = '1' * 65533
    test_sql = """
        set bail on;
        set heading off;
        savepoint a;
        savepoint b;
        savepoint c;
        release savepoint a only;
        release savepoint b only;
        release savepoint c only;
        select 'Completed.' from rdb$database;
    """
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
