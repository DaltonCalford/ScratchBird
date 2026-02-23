#coding:utf-8

"""
ID:          issue-2632
ISSUE:       2632
TITLE:       Constraints on sp output parameters are checked even when the sp returns zero rows
DESCRIPTION:
JIRA:        CORE-2204
FBTEST:      bugs.core_2204
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create domain dm_boo as integer not null check (value = 0 or value = 1);
    set term ^;
    create procedure test returns (b dm_boo) as
    begin
        if (1 = 0) then
            suspend;
    end
    ^
    set term ;^
    commit;
    set count on;
    select * from test;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Records affected: 0
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

