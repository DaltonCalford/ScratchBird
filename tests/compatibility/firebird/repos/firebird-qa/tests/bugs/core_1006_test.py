#coding:utf-8

"""
ID:          issue-1417
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1417
TITLE:       AV at rollback and/or garbage collection if updated table have expression index with SELECT in it
DESCRIPTION:
JIRA:        CORE-1006
FBTEST:      bugs.core_1006
NOTES:
    [13.05.2023] pzotov
    On 5.0.0.1047 test script causes error:
        Statement failed, SQLSTATE = 42000
        Expression evaluation error for index "TABLE1_IDX1" on table "TABLE1"
        -Attempt to evaluate index expression recursively
        -At block line: 17, col: 13
    Added SELECT statement to check its output in assert (otherwise error message remains unclear:
    "firebird.qa.plugin.ExecutionError: Test script execution failed").
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='core1006.fbk')

test_script = """
    set bail on;
    set heading off;
    set term ^;
    execute block
    as
        declare f1 int;
        declare f2 int;
    begin
        for
            select
                t1.id
                    as id1 -- <<< ::: NB ::: add alias, otherwise can`t compile in 3.0
                ,t2.id
                    as id2 -- <<< ::: NB ::: add alias, otherwise can`t compile in 3.0
            from table1 t1, table2 t2
            where t1.id = t2.id
            into :f1, :f2
            as cursor cur
        do
            update table1 set name = :f1 + :f2 where current of cur;
    end
    ^ set term ;^
    rollback;
    select 'Done.' as msg from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    Done.
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
