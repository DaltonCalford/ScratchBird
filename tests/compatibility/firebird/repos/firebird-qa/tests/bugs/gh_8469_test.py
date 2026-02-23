#coding:utf-8

"""
ID:          issue-8437
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8437
TITLE:       Throw exception for non-existing date in string to datetime conversion
DESCRIPTION:

NOTES:
    [27.03.2025] pzotov
    Confirmed bug on 6.0.0.656-25fb454: wrong date was silently ignored and 'next day' was shown:
        0001-10-01 01:01:01.0001
        9999-03-01 00:00:00.0000
    Checked fix on 6.0.0.698-6c21404.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select 
        cast('1-09-31 1:1:1.1' as timestamp format 'yyyy-mm-dd hh24:mi:ss.ff4')
    from rdb$database
    ;

    select 
        cast('9999-2-29' as timestamp format 'yyyy-mm-dd')
    from rdb$database
    ;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    Statement failed, SQLSTATE = 22018
    conversion error from string "1-09-31 1:1:1.1"
    
    Statement failed, SQLSTATE = 22018
    conversion error from string "9999-2-29"
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

