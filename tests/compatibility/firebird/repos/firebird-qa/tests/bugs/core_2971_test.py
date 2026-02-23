#coding:utf-8

"""
ID:          issue-3353
ISSUE:       3353
TITLE:       Invalid UPDATE OR INSERT usage may lead to successive "request depth exceeded. (Recursive definition?)" error
DESCRIPTION:
JIRA:        CORE-2971
FBTEST:      bugs.core_2971
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- This syntax works fine since 2.0.x:
    recreate view v(x, y) as select 1 x, 2 y from rdb$database;
    commit;
    set term ^;
    execute block as
    begin
      execute statement 'drop sequence g';
      when any do begin end
    end
    ^
    set term ;^
    commit;

    create sequence g;
    recreate table t1 (n1 integer);
    recreate table t2 (n2 integer);

    recreate view v(x, y) as select t1.n1 as x, t2.n2 as y from t1, t2;
    commit;

    set term ^;
    execute block as
      declare n int = 1000;
    begin
        while (n>0) do
        begin
            n = n - 1;
            execute statement ('update or insert into v values ( '|| gen_id(g,1) ||', gen_id(g,1))');
            -- ^
            -- For this statement trace 2.5 and 3.0 shows:
            -- ERROR AT JStatement::prepare
            -- ...
            -- 335544569 : Dynamic SQL Error
            -- 336003101 : UPDATE OR INSERT without MATCHING could not be used with views based on more than one table
            -- We have to suppress ONLY exception with gdscode = 335544569 and raise if its gdscode has any other value.
        when any do
            begin
              if  ( gdscode not in (335544569) ) then exception;
            end
        end

    end
    ^
    set term ;^
    rollback;
    set list on;
    select gen_id(g, 0) curr_gen from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CURR_GEN                        1000
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

