#coding:utf-8

"""
ID:          issue-481
ISSUE:       481
TITLE:       Sqlsubtype incorrect on timestamp math, constant arithmetic
DESCRIPTION:
JIRA:        CORE-152
FBTEST:      bugs.core_152
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set sqlda_display;
    set list on;
    select current_timestamp - current_timestamp dts_diff from rdb$database;
"""

substitutions = [('^((?!sqltype|DTS_DIFF).)*$', ''), ('[ ]+', ' '), ('[\t]*', ' ')]

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout = """
    01: sqltype: 580 INT64 scale: -9 subtype: 1 len: 8
    :  name: SUBTRACT  alias: DTS_DIFF
    DTS_DIFF 0.000000000
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

