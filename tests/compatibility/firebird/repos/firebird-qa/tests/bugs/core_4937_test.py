#coding:utf-8

"""
ID:          issue-5228
ISSUE:       5228
TITLE:       View/subselect with "union" does not use computed index
DESCRIPTION:
JIRA:        CORE-4937
FBTEST:      bugs.core_4937
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='mon-stat-gathering-2_5.fbk')

test_script = """
    -- ::: NB ::: Plans in 2.5 and 3.0 differ (see below).
    -- It was decided to check number of natural reads gathered
    -- from mon$ statistics rather than filter plan text with
    -- finding word 'NATURAL'.

    -- Checked on:
    -- 2.5.5.26916 - had non-zero natural reads in statistics
    -- 2.5.5.26933, 3.0.0.32052 - all Ok.
    set term ^;
    execute block as begin execute statement 'drop sequence g'; when any do begin end end
    ^
    set term ;^
    commit;
    create sequence g;

    recreate view test_view as select 1 id from rdb$database;
    recreate table test1 (id int not null primary key, tms timestamp default current_timestamp);
    recreate table test2 (id int not null primary key, tms timestamp default current_timestamp);

    alter table test1 add occurred int computed by (case when tms < current_timestamp then 1 else 0 end);

    alter table test2 add occurred int;

    recreate view test_view as select * from test1 union select * from test2;
    commit;

    insert into test1(id, tms)
    select gen_id(g,1), dateadd( -gen_id(g,1) minute to cast('now' as timestamp) )
    from rdb$types
    rows 200;

    insert into test1(id, tms)
    select gen_id(g,1), dateadd( gen_id(g,1) minute to cast('now' as timestamp) )
    from rdb$types
    rows 200;


    insert into test2(id, tms)
    select gen_id(g,1), dateadd( -gen_id(g,1) minute to cast('now' as timestamp) )
    from rdb$types
    rows 200;

    insert into test2(id, tms)
    select gen_id(g,1), dateadd( gen_id(g,1) minute to cast('now' as timestamp) )
    from rdb$types
    rows 200;

    update test2 set occurred = case when tms < current_timestamp then 1 else 0 end;

    commit;

    create index test1_occ on test1 computed by (case when tms < current_timestamp then 1 else 0 end);
    create index test2_occ on test2(occurred);
    commit;

    execute procedure sp_truncate_stat;
    commit;

    set list on;

    --------------------- run-1 -------------------
    execute procedure sp_gather_stat; -- catch statistics BEFORE measured statement(s)
    commit;

    select count(*) cnt_1 from test1 where occurred = 1;

    execute procedure sp_gather_stat; -- catch statistics AFTER measured statement(s)
    commit;

    --------------------- run-2 -------------------
    execute procedure sp_gather_stat; -- catch statistics BEFORE measured statement(s)
    commit;

    select count(*) cnt_2 from test2 where occurred = 1;

    execute procedure sp_gather_stat; -- catch statistics AFTER measured statement(s)
    commit;

    --------------------- run-3 -------------------
    execute procedure sp_gather_stat; -- catch statistics BEFORE measured statement(s)
    commit;

    -- in 2.5:
    -- PLAN (TEST1 INDEX (TEST1_OCC))
    -- PLAN (TEST2 INDEX (TEST2_OCC))
    -- in 3.0:
    -- PLAN SORT (TEST1 INDEX (TEST1_OCC), TEST2 INDEX (TEST2_OCC))

    select count(*) cnt_3 from (select * from test1 union select * from test2) where occurred = 1;

    execute procedure sp_gather_stat; -- catch statistics AFTER measured statement(s)
    commit;

    --------------------- run-4 -------------------
    execute procedure sp_gather_stat; -- catch statistics BEFORE measured statement(s)
    commit;

    --  in 2.5:
    -- PLAN (TEST_VIEW TEST1 INDEX (TEST1_OCC))
    -- PLAN (TEST_VIEW TEST2 INDEX (TEST2_OCC))
    -- in 3.0:
    -- PLAN SORT (TEST_VIEW TEST1 INDEX (TEST1_OCC), TEST_VIEW TEST2 INDEX (TEST2_OCC))

    select count(*) cnt_4 from test_view where occurred = 1;

    execute procedure sp_gather_stat; -- catch statistics AFTER measured statement(s)
    commit;

    -------------------------------------------------

    -- Output statistics. Natural reads in all cases should be 0 (zero):
    select 'run_' || a.rowset as run_no, a.natural_reads from v_agg_stat a;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CNT_1                           200
    CNT_2                           200
    CNT_3                           400
    CNT_4                           400

    RUN_NO                          run_1
    NATURAL_READS                   0
    RUN_NO                          run_2
    NATURAL_READS                   0
    RUN_NO                          run_3
    NATURAL_READS                   0
    RUN_NO                          run_4
    NATURAL_READS                   0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

