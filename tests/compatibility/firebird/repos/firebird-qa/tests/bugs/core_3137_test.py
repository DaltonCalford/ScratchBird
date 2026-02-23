#coding:utf-8

"""
ID:          issue-3514
ISSUE:       3514
TITLE:       Partial rollback is possible for a selectable procedure modifying data
DESCRIPTION:
JIRA:        CORE-3137
FBTEST:      bugs.core_3137
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter procedure sp_01 returns (ret int) as begin end;
    commit;
    recreate table tab (col int);
    commit;
    insert into tab (col) values (1);
    commit;

    set term ^;
    create or alter procedure sp_01 returns (ret int) as
    begin
        update tab set col = 2;
        begin
            update tab set col = 3;
            ret = 1;
            suspend;
        end
        when any do
        begin
            ret = 0;
            suspend;
        end
    end
    ^ set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select col from tab; -- returns 1
    commit;

    select ret from sp_01;
    rollback;

    select col from tab; -- returns 2!!!
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    COL                             1
    RET                             1
    COL                             1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

