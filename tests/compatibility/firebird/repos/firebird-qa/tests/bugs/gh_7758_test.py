#coding:utf-8

"""
ID:          issue-7427
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/7427
TITLE:       Added the ability to change function DETERMINISTIC option without recompile the entire function body
DESCRIPTION: 
    ALTER FUNCTION <name> {DETERMINISTIC | NOT DETERMINISTIC}
    doc/sql.extensions/README.ddl.txt
NOTES:
    [09.10.2023] pzotov
    1. Test verifies ability to change DETERMINISTIC attribute and actual behaviour of function after this action.
    2. ::: NB ::: Currently one need to make RECONNECT after this change, otherwise behaviour remains unchanged
       (see: https://github.com/FirebirdSQL/firebird/issues/7427#issuecomment-1753095418 ).

    Checked on 6.0.0.76.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create sequence g start with 0;
    commit;
    set term ^;
    create function fn_test returns int deterministic
    as
    begin
        return gen_id(g,1);
    end
    ^
    commit
    ^

    create procedure sp_test returns(curr_gen int) as
        declare n int = 10;
        declare i int;
    begin
        while (n > 0) do
        begin
            i = fn_test();
            n = n - 1;
        end
        curr_gen = gen_id(g,0);
        suspend;
    end
    ^
    select p.curr_gen as gen_determ_initial from sp_test p
    ^
    commit
    ^
    alter sequence g restart with 0
    ^
    alter function fn_test not deterministic
    ^
    commit
    ^
    CONNECT $(DSN)
    ^
    select p.curr_gen as gen_non_deterministic from sp_test p
    ^
    alter sequence g restart with 0
    ^
    alter function fn_test deterministic
    ^
    commit
    ^
    CONNECT $(DSN)
    ^
    select p.curr_gen as gen_determ_returned from sp_test p
    ^

"""

act = isql_act('db', test_script)

expected_stdout = """
    GEN_DETERM_INITIAL              0
    GEN_NON_DETERMINISTIC           9
    GEN_DETERM_RETURNED             0
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
