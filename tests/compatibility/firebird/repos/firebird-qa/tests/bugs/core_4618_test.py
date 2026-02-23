#coding:utf-8

"""
ID:          issue-4933
ISSUE:       4933
TITLE:       Rollback doesn`t undo changes when MERGE statement updates the same target
  rows multiple times and PLAN MERGE is used
DESCRIPTION:
NOTES:
[07.06.2020]
  separate section for FB 4.x was added since fix for core-2274 issued:
    MERGE can not change the same record multiple times.
  For this reason we have to check only presense of ERROR in 4.x and that result is the
  same after merge and rollback.
JIRA:        CORE-4618
FBTEST:      bugs.core_4618
"""

import pytest
from firebird.qa import *

# version: 3.0

db_1 = db_factory()

test_script_1 = """
    set term ^;
    execute block as begin
      begin execute statement 'create sequence g'; when any do begin end end
    end
    ^ set term ;^
    commit;
    alter sequence g restart with 0;
    commit;
    recreate table t(id int, x int, y int);
    commit;
    insert into t(id) select gen_id(g,1) from rdb$types rows 3;
    update t set x=mod(id,2), y=mod(id,3);
    commit;

    select 'before_merge' msg, t.* from t;

    --set plan on;
    merge into t
    using t s
    on t.x=s.x
    when matched then update set t.x = t.x+s.y, t.y = t.y - s.x;
    set plan off;

    select 'after_merge' msg, t.* from t;

    rollback;

    select 'after_rollback' msg, t.* from t;

    -- ::: NB ::: Seems that trouble was NOT only because of PLAN MERGE.
    -- Compare with WI-T3.0.0.31374 Firebird 3.0 Beta 1 - here HASH also is used!
    --    MSG                    ID            X            Y
    --    ============ ============ ============ ============
    --    before_merge            1            1            1
    --    before_merge            2            0            2
    --    before_merge            3            1            0
    --
    --    PLAN HASH (T NATURAL, S NATURAL)
    --
    --    MSG                   ID            X            Y
    --    =========== ============ ============ ============
    --    after_merge            1            2           -1
    --    after_merge            2            2            2
    --    after_merge            3            2           -2
    --
    --    MSG                      ID            X            Y
    --    ============== ============ ============ ============
    --    after_rollback            1            2         -255
    --    after_rollback            2            0            2
    --    after_rollback            3            2            0
"""

act_1 = isql_act('db_1', test_script_1, substitutions=[('=.*', '')])

expected_stdout_1 = """
    MSG                    ID            X            Y
    ============ ============ ============ ============
    before_merge            1            1            1
    before_merge            2            0            2
    before_merge            3            1            0

    MSG                   ID            X            Y
    =========== ============ ============ ============
    after_merge            1            2            0
    after_merge            2            2            2
    after_merge            3            2           -1

    MSG                      ID            X            Y
    ============== ============ ============ ============
    after_rollback            1            1            1
    after_rollback            2            0            2
    after_rollback            3            1            0
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout_1
    act_1.execute()
    assert act_1.clean_stdout == act_1.clean_expected_stdout

# version: 4.0

db_2 = db_factory()

test_script_2 = """
    recreate table test(id int, x int, y int);
    create descending index test_x_dec on test(x);
    commit;
    insert into test(id, x, y) values(1, 1, 1);
    insert into test(id, x, y) values(2, 0, 2);
    insert into test(id, x, y) values(3, 1, 0);
    commit;

    select 'before_merge' msg, t.* from test t;

    merge into test t
    using test s on t.x=s.x
    when matched then update set t.x = t.x+s.y, t.y = t.y - s.x;

    select 'after_merge' msg, t.* from test t;
    rollback;
    select 'after_rollback' msg, t.* from test t;
"""

act_2 = isql_act('db_2', test_script_2, substitutions=[('[ \t]+', ' '), ('=', '')])

expected_stdout_2 = """
    MSG                    ID            X            Y
    ============ ============ ============ ============
    before_merge            1            1            1
    before_merge            2            0            2
    before_merge            3            1            0


    MSG                   ID            X            Y
    =========== ============ ============ ============
    after_merge            1            1            1
    after_merge            2            0            2
    after_merge            3            1            0


    MSG                      ID            X            Y
    ============== ============ ============ ============
    after_rollback            1            1            1
    after_rollback            2            0            2
    after_rollback            3            1            0
"""

expected_stderr_2 = """
    Statement failed, SQLSTATE = 21000
    Multiple source records cannot match the same target during MERGE
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout_2
    act_2.expected_stderr = expected_stderr_2
    act_2.execute()
    assert (act_2.clean_stderr == act_2.clean_expected_stderr and
            act_2.clean_stdout == act_2.clean_expected_stdout)

