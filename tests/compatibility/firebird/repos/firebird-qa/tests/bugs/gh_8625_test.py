#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8625
TITLE:       Range based FOR is broken with a DO SUSPEND without BEGIN...END
DESCRIPTION:
NOTES:
    [17.07.2025] pzotov
    Confirmed problem on 6.0.0.845.
    Checked on 6.0.0.1020
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    set autoterm;
    execute block returns (i int) as
    begin
        for i = 1 to 3 do suspend;
    end;
"""

act = isql_act('db', test_script)

expected_stdout = """
    1
    2
    3
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
