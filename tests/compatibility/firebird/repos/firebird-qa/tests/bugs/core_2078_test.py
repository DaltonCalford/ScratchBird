#coding:utf-8

"""
ID:          issue-2513
ISSUE:       2513
TITLE:       Suboptimal join plan if there are selective non-indexed predicates involved
DESCRIPTION:
  This test operates with three tables: "small", "medium" and "big" - which are INNER-join'ed.
  It was found that there is some threshold ratio between number of rows in "small"  vs "medium"
  tables which affects on generated PLAN after reaching this ratio.
  In particular, when tables  have following values of rows: 26, 300 and 3000 - than optimizer
  still DOES take in account "WHERE" condition with non-indexed field in SMALL table ("where s.sf = 0"),
  and this lead to GOOD (fast) performance because SMALL table will be FIRST in the join order.
  However, if number of rows in SMALL table will change from 26 to 27 (yes, just one row) than
  optimizer do NOT consider additional condition (WHERE-filter) on that table and begin choose
  SLOW (ineffective) plan where MEDIUM table is first in join order.
  After discussion with dimitr, it was decided to:
    1) put here TWO cases of query: with "fast" and "slow" plan;
    2) replace too narrow threshold-pair (26 vs 27) with more wider (now: 15 and 45) because otherwise
       even minor changes in optimizer can breake this test expected output.
  Test make TWO PAIRS (i.e. total FOUR) runs:
    1. When number of rows in SMALL table is less than threshold:
    1.1. Without 'where'-filter on small table - to ensure that plan will contain MEDIUM table as first in join;
    1.2. WITH 'where'-filter on small table - to ensure that plan will be CHANGED and SMALL table will be first;
    2. When number of rows in SMALL table is beyond the threshold:
    2.1. Without 'where'-filter on small table - plan will contain MEDIUM table as first in join;
    2.2. WITH 'where'-filter on small table - plan will NOT changed, MEDIUM table will remain first in join.
  Beside output of PLAN itself, test also:
    1) displays index statistics
    2) compares fetches with some 'upper-limit' constants in order to alert us in case when fetches become too high.
  These constants have been obtained after sereval experiments with page_size = 4k, and their values are base on
  following typical results (which are the same on 2.5 and 3.0):
  FETCHES_1_1                     19636
  FETCHES_1_2                     9094
  FETCHES_2_1                     19548
  FETCHES_2_2                     19548
NOTES:
  [18.08.2020]
    Test uses pre-created database which has several procedures for analyzing performance by with the help of MON$ tables.
    Performance results are gathered in the table STAT_LOG, each odd run will save mon$ counters with "-" sign and next
    (even) run will save them with "+" -- see SP_GATHER_STAT.
    Aggegation of results is done in the view V_AGG_STAT (negative values relate to start, positive to the end of measure,
    difference between them means performance expenses which we want to evaluate).
    NOTE. Before each new measure we have to set generator G_GATHER_STAT to zero in order to make it produce proper values
    starting with 1 (odd --> NEGATIVE sign for counters). This is done in SP_TRUNCATE_STAT.
  
  [18.08.2020]
    FB 4.x has incompatible behaviour with all previous versions since build 4.0.0.2131 (06-aug-2020):
    statement 'alter sequence <seq_name> restart with 0' changes rdb$generators.rdb$initial_value to -1 thus next call
    gen_id(<seq_name>,1) will return 0 (ZERO!) rather than 1.
    See also CORE-6084 and its fix: https://github.com/FirebirdSQL/firebird/commit/23dc0c6297825b2e9006f4d5a2c488702091033d

    This is considered as *expected* and is noted in doc/README.incompatibilities.3to4.txt

    Because of this, it was decided to change code of SP_TRUNCATE_STAT: instead of 'alter sequence restart...' we do
    reset like this: c = gen_id(g_gather_stat, -gen_id(g_gather_stat, 0));

  [16.12.2023]
  Removed output of execution plans after note by dimitr (letter 15.12.2023 10:05): this test probably needs to be re-implemented
  because initially it was designed for check NESTED LOOPS. To be discussed furter.


JIRA:        CORE-2078
FBTEST:      bugs.core_2078
"""

import pytest
from firebird.qa import *

db = db_factory(from_backup='mon-stat-gathering-3_0.fbk')

test_script = """
    create or alter procedure sp_fill_data(a_sml_rows int, a_med_rows int, a_big_rows int) as begin end;
    recreate table tbig(id int, sid int, mid int);
    commit;
    recreate table tsml(id int not null, sf int);
    commit;
    recreate table tmed(id int not null);
    commit;

    set term ^;
    create or alter procedure sp_fill_data(a_sml_rows int, a_med_rows int, a_big_rows int)
    as
      declare i int;
      declare i_mod2 smallint;
    begin

      -- gather old record versions if they are from previous run:
      select count(*) from tsml into i;
      select count(*) from tmed into i;
      select count(*) from tbig into i;

      i=0;
      while (i < a_sml_rows) do
      begin
         insert into tsml(id, sf) values( :i, :i - (:i/2)*2 );
         i = i+1;
      end

      i=0;
      while (i < a_med_rows) do
      begin
         insert into tmed(id) values( :i );
         i = i+1;
      end

      i=0;
      while (i < a_big_rows) do
      begin
         insert into tbig(id, sid, mid) values( :i, :i - (:i/2)*2, :i - (:i/:a_med_rows)*:a_med_rows );
         i = i+1;
      end

    end
    ^

    create or alter procedure srv_recalc_idx_stat
    returns (
        tab_name varchar(31),
        idx_name varchar(31),
        idx_stat_afte numeric(12, 10) -- double precision
    )
    as
    begin
        for
            select ri.rdb$relation_name, ri.rdb$index_name
            from rdb$indices ri
            join rdb$relations rr on ri.rdb$relation_name = rr.rdb$relation_name
            where
                coalesce(ri.rdb$system_flag,0)=0
                and rr.rdb$relation_type = 0 -- exclude GTTs!
            order by ri.rdb$relation_name, ri.rdb$index_name
        into
            tab_name, idx_name
        do begin
            execute statement( 'set statistics index '||idx_name )
            with autonomous transaction
            ;

            select ri.rdb$statistics
            from rdb$indices ri
            where ri.rdb$relation_name = :tab_name and ri.rdb$index_name = :idx_name
            into idx_stat_afte;

            suspend;
        end
    end
    ^

    set term ;^
    commit;

    alter table tsml add constraint tsml_pk primary key(id) using index tsml_pk;
    alter table tmed add constraint tmed_pk primary key(id) using index tmed_pk;

    alter table tbig add constraint tbig_fk_sml foreign key(sid) references tsml using index tbig_idx1_fk_sml;
    alter table tbig add constraint tbig_fk_med foreign key(mid) references tmed using index tbig_idx2_fk_med;
    commit;

    set width tab_name 31;
    set width idx_name 31;
    set list on;

    -------------------- prepare-1 ------------------
    --execute procedure sp_fill_data(26, 300, 3000);
    execute procedure sp_fill_data(15, 300, 3000);
    commit;

    set transaction read committed;
    select
        tab_name as run1_tab_name
       ,idx_name as run1_idx_name
       ,idx_stat_afte run1_idx_stat
    from srv_recalc_idx_stat where tab_name in ( upper('tsml'), upper('tmed'), upper('tbig') );
    commit;

    --------------------- run-1.1 -------------------
    -----alter sequence g_gather_stat restart with 0;
    execute procedure sp_truncate_stat;
    commit;

    execute procedure sp_gather_stat; ------- catch statistics BEFORE measured statement(s)
    commit;

    --set plan on;
    select count(*) cnt_1_1
    from tsml s
      join tbig b on b.sid = s.id
      join tmed m on b.mid = m.id
    ;
    --set plan off;

    execute procedure sp_gather_stat;  ------- catch statistics AFTER measured statement(s)
    commit;

    --------------------- run-1.2 -------------------
    execute procedure sp_gather_stat; ------- catch statistics BEFORE measured statement(s)
    commit;

    --set plan on;
    select count(*) cnt_1_2
    from tsml s
      join tbig b on b.sid = s.id
      join tmed m on b.mid = m.id
    where s.sf = 0  -- selective non-indexed boolean
    ;
    --set plan off;

    execute procedure sp_gather_stat;  ------- catch statistics AFTER measured statement(s)
    commit;

    -------------------- prepare-2 ------------------
    delete from tbig;
    delete from tmed;
    delete from tsml;
    commit;

    --execute procedure sp_fill_data(27, 300, 3000);
    execute procedure sp_fill_data(45, 300, 3000);
    commit;

    set transaction read committed;
    select
        tab_name as run2_tab_name
       ,idx_name as run2_idx_name
       ,idx_stat_afte run2_idx_stat
    from srv_recalc_idx_stat where tab_name in ( upper('tsml'), upper('tmed'), upper('tbig') );
    commit;

    --------------------- run-2.1 -------------------
    execute procedure sp_gather_stat; ------- catch statistics BEFORE measured statement(s)
    commit;

    --set plan on;
    select count(*) cnt_2_1
    from tsml s
        join tbig b on b.sid = s.id
        join tmed m on b.mid = m.id
    ;
    --set plan off;

    execute procedure sp_gather_stat;  ------- catch statistics AFTER measured statement(s)
    commit;
    --------------------- run-2.2 -------------------

    execute procedure sp_gather_stat; ------- catch statistics BEFORE measured statement(s)
    commit;

    --set plan on;
    select count(*) cnt_2_2
    from tsml s
        join tbig b on b.sid = s.id
        join tmed m on b.mid = m.id
    where s.sf = 0  -- selective non-indexed boolean
    ;
    --set plan off;
    
    execute procedure sp_gather_stat;  ------- catch statistics AFTER measured statement(s)
    commit;


    -- Here we define constants that serve as *upper* limit for fetches:
    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION','MAX_FETCHES_FOR_SLOW', 22000);
        rdb$set_context('USER_SESSION','MAX_FETCHES_FOR_FAST', 11000);
    end
    ^
    set term ;^

    -- Typical values for page_size 4K on 2.5 and 3.0:
    -- FETCHES_1_1                     19636
    -- FETCHES_1_2                     9094
    -- FETCHES_2_1                     19548
    -- FETCHES_2_2                     19548

    -- Show results:
    -- =============
    select
        iif( fetches_1_1 <= max_fetches_for_slow, 'acceptable', 'regression: '|| fetches_1_1 || ' > ' || max_fetches_for_slow) as fetches_1_1
       ,iif( fetches_1_2 <= max_fetches_for_fast, 'acceptable', 'regression: '|| fetches_1_2 || ' > ' || max_fetches_for_fast ) as fetches_1_2
       ,iif( fetches_2_1 <= max_fetches_for_slow, 'acceptable', 'regression: '|| fetches_2_1 || ' > ' || max_fetches_for_slow ) as fetches_2_1
       ,iif( fetches_2_2 <= max_fetches_for_slow, 'acceptable', 'regression: '|| fetches_2_2 || ' > ' || max_fetches_for_slow ) as fetches_2_2
    from (
        select
            max( iif(rowset = 1, page_fetches, null) ) fetches_1_1
           ,max( iif(rowset = 2, page_fetches, null) ) fetches_1_2
           ,max( iif(rowset = 3, page_fetches, null) ) fetches_2_1
           ,max( iif(rowset = 4, page_fetches, null) ) fetches_2_2
           ,cast(rdb$get_context('USER_SESSION','MAX_FETCHES_FOR_SLOW') as int) as max_fetches_for_slow
           ,cast(rdb$get_context('USER_SESSION','MAX_FETCHES_FOR_FAST') as int) as max_fetches_for_fast
        from v_agg_stat
    )
    ;
"""

act = isql_act('db', test_script)

fb3x_expected_out = """
    RUN1_TAB_NAME                   TBIG
    RUN1_IDX_NAME                   TBIG_IDX1_FK_SML
    RUN1_IDX_STAT                   0.5000000000

    RUN1_TAB_NAME                   TBIG
    RUN1_IDX_NAME                   TBIG_IDX2_FK_MED
    RUN1_IDX_STAT                   0.0033333334

    RUN1_TAB_NAME                   TMED
    RUN1_IDX_NAME                   TMED_PK
    RUN1_IDX_STAT                   0.0033333334

    RUN1_TAB_NAME                   TSML
    RUN1_IDX_NAME                   TSML_PK
    RUN1_IDX_STAT                   0.0666666701

    CNT_1_1                         3000

    CNT_1_2                         1500

    RUN2_TAB_NAME                   TBIG
    RUN2_IDX_NAME                   TBIG_IDX1_FK_SML
    RUN2_IDX_STAT                   0.5000000000

    RUN2_TAB_NAME                   TBIG
    RUN2_IDX_NAME                   TBIG_IDX2_FK_MED
    RUN2_IDX_STAT                   0.0033333334

    RUN2_TAB_NAME                   TMED
    RUN2_IDX_NAME                   TMED_PK
    RUN2_IDX_STAT                   0.0033333334

    RUN2_TAB_NAME                   TSML
    RUN2_IDX_NAME                   TSML_PK
    RUN2_IDX_STAT                   0.0222222228

    CNT_2_1                         3000

    CNT_2_2                         1500

    FETCHES_1_1                     acceptable
    FETCHES_1_2                     acceptable
    FETCHES_2_1                     acceptable
    FETCHES_2_2                     acceptable
"""

fb5x_expected_out = """
    RUN1_TAB_NAME                   TBIG                           
    RUN1_IDX_NAME                   TBIG_IDX1_FK_SML               
    RUN1_IDX_STAT                   0.5000000000

    RUN1_TAB_NAME                   TBIG                           
    RUN1_IDX_NAME                   TBIG_IDX2_FK_MED               
    RUN1_IDX_STAT                   0.0033333334

    RUN1_TAB_NAME                   TMED                           
    RUN1_IDX_NAME                   TMED_PK                        
    RUN1_IDX_STAT                   0.0033333334

    RUN1_TAB_NAME                   TSML                           
    RUN1_IDX_NAME                   TSML_PK                        
    RUN1_IDX_STAT                   0.0666666701

    CNT_1_1                         3000

    CNT_1_2                         1500

    RUN2_TAB_NAME                   TBIG                           
    RUN2_IDX_NAME                   TBIG_IDX1_FK_SML               
    RUN2_IDX_STAT                   0.5000000000

    RUN2_TAB_NAME                   TBIG                           
    RUN2_IDX_NAME                   TBIG_IDX2_FK_MED               
    RUN2_IDX_STAT                   0.0033333334

    RUN2_TAB_NAME                   TMED                           
    RUN2_IDX_NAME                   TMED_PK                        
    RUN2_IDX_STAT                   0.0033333334

    RUN2_TAB_NAME                   TSML                           
    RUN2_IDX_NAME                   TSML_PK                        
    RUN2_IDX_STAT                   0.0222222228

    CNT_2_1                         3000

    CNT_2_2                         1500
    FETCHES_1_1                     acceptable
    FETCHES_1_2                     acceptable
    FETCHES_2_1                     acceptable
    FETCHES_2_2                     acceptable
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = fb3x_expected_out if act.is_version('<5') else fb5x_expected_out
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
