#coding:utf-8

"""
ID:          issue-4625-A
ISSUE:       4625
TITLE:       Lookup (or scan) in descending index could be very inefficient for some keys
DESCRIPTION:
  1. Bug of this ticket was introduced _after_ FB 3.0 Alpha1 release and was fixed 21-dec-2013.
     I could not reproduce poor statistics (too big value of fetches) on WI-T3.0.0.30566 (Alpha1 release),
     so temporary build was created based on snapshot of 20-dec-2013 (on POSIX): LI-T3.0.0.30800.
     This snapshot DOES reproduce problem: number of fetches for val=1 or val=3 is less than 30
     while for val=2 produces number of fetches more than 210'000.
     For 2.5.x number of fetches is about 59...67, so common threshold was choosen to value 70.
  2. Test was refactored: querying to database 'mon-stat-gathering-N_M.fdb' was removed, all necessary
     output one may achieve by trivial view (v_mon) that queries mon$io_stats and and table (t_mon) that
     serves as log.
JIRA:        CORE-4302
FBTEST:      bugs.core_4302
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
     recreate table t_mon(rn smallint, pg_fetches bigint);

     recreate view v_mon as
     select i.mon$page_fetches as pg_fetches
     from mon$attachments a
     left join mon$io_stats i on a.mon$stat_id=i.mon$stat_id
     where a.mon$attachment_id = current_connection;

     recreate table t1 (id int, val int);
     commit;

     set term ^;
     execute block as
     declare i int = 0;
     begin
       while (i < 100000) do
       begin
         insert into t1 values (:i, mod(:i, 10));
         i = i + 1;
       end
     end
     ^
     set term ;^
     commit;

     create descending index t1_desc_idx on t1 (val);
     commit;

     set list on;

     insert into t_mon(rn, pg_fetches) values( 1, (select pg_fetches from v_mon));
     select * from t1 where val <= 1 order by val desc rows 1;
     commit;
     insert into t_mon(rn, pg_fetches) values( 2, (select pg_fetches from v_mon));
     commit;

     insert into t_mon(rn, pg_fetches) values( 3, (select pg_fetches from v_mon));
     select * from t1 where val <= 2 order by val desc rows 1;
     commit;
     insert into t_mon(rn, pg_fetches) values( 4, (select pg_fetches from v_mon));
     commit;

     insert into t_mon(rn, pg_fetches) values( 5, (select pg_fetches from v_mon));
     select * from t1 where val <= 3 order by val desc rows 1;
     commit;
     insert into t_mon(rn, pg_fetches) values( 6, (select pg_fetches from v_mon));
     commit;

     select
          rn as measure
         ,iif( fetches_diff < max_allowed, 'OK, less then ', 'BAD: '|| fetches_diff ||' - greater then ')  || max_allowed as fetches_count
     from (
         select
             rn
            ,fetches_at_end - fetches_at_beg as fetches_diff
            ,70 as max_allowed
         --  ^        ###################################################
         --  |        #                                                 #
         --  +------- #   T H R E S H O L D     F O R    F E T C H E S  #
         --           #                                                 #
         --           ###################################################
         from (
           select rn, max(iif(bg=1, pg_fetches, null)) as fetches_at_beg, max(iif(bg=1, null, pg_fetches)) as fetches_at_end
           from (
               select 1+(rn-1)/2 as rn, mod(rn,2) as bg, pg_fetches
               from t_mon
           )
           group by rn
         )
     )
     order by measure;
"""

act = isql_act('db', test_script, substitutions=[('[ ]+', ' ')])

expected_stdout = """
    ID                              1
    VAL                             1
    ID                              2
    VAL                             2
    ID                              3
    VAL                             3

    MEASURE                         1
    FETCHES_COUNT                   OK, less then 70

    MEASURE                         2
    FETCHES_COUNT                   OK, less then 70

    MEASURE                         3
    FETCHES_COUNT                   OK, less then 70

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

