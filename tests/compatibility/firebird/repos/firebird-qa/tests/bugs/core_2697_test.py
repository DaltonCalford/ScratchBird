#coding:utf-8

"""
ID:          issue-3097
ISSUE:       3097
TITLE:       Support the "? in (SELECT some_col FROM some_table)" subqueries
DESCRIPTION:
JIRA:        CORE-2697
FBTEST:      bugs.core_2697
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table t1(id int, x int);
    commit;
    recreate table t2(id int, t1_id int, y int);
    commit;
    insert into t1 values(1, 100);
    insert into t1 values(2, 200);
    insert into t1 values(3, 300);

    insert into t2 values(11, 1, 111);
    insert into t2 values(12, 1, 112);
    insert into t2 values(13, 1, 113);
    insert into t2 values(33, 3, 333);
    insert into t2 values(34, 3, 334);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set term ^;
    execute block returns(x_in_t2 int) as
        declare v_stt varchar(255);
    begin
        v_stt =
            'select t1.x '
            || 'from t1 '
            || 'where t1.x > 0 '
            || 'and ( ? in ( select mod(t2.id, 2) from t2 where t1.id = t2.t1_id ) )';
        for
            execute statement ( v_stt ) ( 0 ) into x_in_t2
        do
            suspend;
    end
    ^
    set term ;^
    -- Output in 2.5:
    --    Statement failed, SQLSTATE = HY004
    --    Dynamic SQL Error
    --    -SQL error code = -804
    --    -Data type unknown
"""

act = isql_act('db', test_script)

expected_stdout = """
    X_IN_T2                         100
    X_IN_T2                         300
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

