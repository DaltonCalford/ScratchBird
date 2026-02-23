#coding:utf-8

"""
ID:          issue-1846
ISSUE:       1846
TITLE:       Incorrect timestamp substraction in 3 dialect when result is negative number
DESCRIPTION:
JIRA:        CORE-1428
FBTEST:      bugs.core_1428
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
        select (cast('2007-08-22 00:00:00.0019' as timestamp) - cast('2007-08-22 00:00:00.0000' as timestamp)) *86400*10000 as dts_01 from rdb$database;
        select (cast('2007-08-22 00:00:00.0000' as timestamp) - cast('2007-08-22 00:00:00.0019' as timestamp)) *86400*10000 as dts_02 from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
        DTS_01 19.008000000
        DTS_02 -19.008000000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

