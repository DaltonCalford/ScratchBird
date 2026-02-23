#coding:utf-8

"""
ID:          issue-3315
ISSUE:       3315
TITLE:       Wrong field position after ALTER POSITION
DESCRIPTION:
JIRA:        CORE-2932
FBTEST:      bugs.core_2932
"""

import pytest
from firebird.qa import *

init_script = """
    create or alter view v_pos as select 1 id from rdb$database;
    commit;

    recreate table test(
        f01 int
        ,f02 int
        ,f03 int
        ,f04 int
        ,f05 int
        ,f06 int
        ,f07 int
        ,f08 int
        ,f09 int
        ,f10 int
    );
    commit;

    create or alter view v_pos as
    -- NB: do NOT use direct values of rdb$field_position, they can on some phase
    -- go with step more than 1 (i.e. not monotonically increased with step = 1).
    -- Use dense_rank()over() instead:
    select
         cast( dense_rank()over(order by rf.rdb$field_position) as smallint ) fld_rnk
        ,rf.rdb$field_name fld_name
    from rdb$relation_fields rf
    where rf.rdb$relation_name = 'TEST'
    order by fld_rnk;
"""

db = db_factory(init=init_script)

test_script = """
    set width msg 6;
    set width fld_rnk 7;
    set width fld_name 8;
    select 'step-1' msg, v.* from v_pos v;

    alter table test drop f01, drop f02, drop f03, drop f04;
    commit;

    alter table test alter f10 position 3;
    -- after this DDL must be: f05 f06 f10 f07 f08 f09
    commit;

    select 'step-2' msg, v.* from v_pos v;

    alter table test add n01 int, add n02 int, add n03 int, add n04 int, add n05 int, add n06 int;
    commit;

    alter table test drop n01, drop n02, drop n03, drop n04;
    -- after this DDL must be: f05 f06 f10 f07 f08 f09 n05 n06
    --                          1   2   3   4   5   6   7   8
    commit;

    select 'step-3' msg, v.* from v_pos v;


    -- Result of this statement "alter table test alter f10 position 7, alter n06 position 3"
    -- can be easy understanded if we split it on subsequent ones:
    -- 1) alter table test alter f10 position 7;
    -- ==> should give:
    --    f05 f06 f07 f08 f09 n05 f10 n06
    --     1   2   3   4   5   6   7   8
    -- 2) alter table test alter n06 position 3;
    -- ==> should give:
    --    f05 f06 n06 f07 f08 f09 n05 f10
    --     1   2   3   4   5   6   7   8
    alter table test alter f10 position 7, alter n06 position 3;
    -- alter table test alter f05 position 8;
    commit;

    select 'step-4' msg, v.* from v_pos v;
"""

act = isql_act('db', test_script, substitutions=[('=.*', '')])

expected_stdout = """
    MSG    FLD_RNK FLD_NAME
    ====== ======= ========
    step-1       1 F01
    step-1       2 F02
    step-1       3 F03
    step-1       4 F04
    step-1       5 F05
    step-1       6 F06
    step-1       7 F07
    step-1       8 F08
    step-1       9 F09
    step-1      10 F10


    MSG    FLD_RNK FLD_NAME
    ====== ======= ========
    step-2       1 F05
    step-2       2 F06
    step-2       3 F10
    step-2       4 F07
    step-2       5 F08
    step-2       6 F09


    MSG    FLD_RNK FLD_NAME
    ====== ======= ========
    step-3       1 F05
    step-3       2 F06
    step-3       3 F10
    step-3       4 F07
    step-3       5 F08
    step-3       6 F09
    step-3       7 N05
    step-3       8 N06


    MSG    FLD_RNK FLD_NAME
    ====== ======= ========
    step-4       1 F05
    step-4       2 F06
    step-4       3 N06
    step-4       4 F07
    step-4       5 F08
    step-4       6 F09
    step-4       7 N05
    step-4       8 F10
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

