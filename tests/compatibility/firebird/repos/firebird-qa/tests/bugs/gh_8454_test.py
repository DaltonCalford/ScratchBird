#coding:utf-8

"""
ID:          issue-8437
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8437
TITLE:       incorrect behavior of FF1...FF4 patterns in CAST FORMAT for string to datetime conversion
DESCRIPTION:

NOTES:
    [27.03.2025] pzotov
    Original ticket title: "Input FORMAT of second fractions"
    Confirmed bug on 6.0.0.656-25fb454.
    Checked fix on 6.0.0.698-6c21404.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select 
        timestamp '1-1-1 1:1:1.1' as source_timestamp_literal,
        cast('1-1-1 1:1:1.1' as timestamp format 'yyyy-mm-dd hh24:mi:ss.ff4') string_as_ts_with_format
    from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    SOURCE_TIMESTAMP_LITERAL  2001-01-01 01:01:01.1000
    STRING_AS_TS_WITH_FORMAT  0001-01-01 01:01:01.1000
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

