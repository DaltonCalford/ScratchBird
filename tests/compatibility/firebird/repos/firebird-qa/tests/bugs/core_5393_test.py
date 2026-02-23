#coding:utf-8

"""
ID:          issue-5666
ISSUE:       5666
TITLE:       Bad optimization of some operations with views containing subqueries
DESCRIPTION:
    Re-implemented 21.11.2021: avoid compare values of datediff(millisecond from ... to ...) because heavy concurrent
    workload can occur at the time when this test is running (encountered multiple cases of this for last year).

    CPU User Time values are used instead, package 'psutil' must be installed for this test.

    Two procedures are created in order to check issue, each with pair of UPDATE statements inside cursor loop:
    * sp_upd_using_primary_key_field - with 1st UPDATE that uses primary key;
    * sp_upd_using_current_of_cursor - with 1st UPDATE that uses 'WHERE CURRENT OF <C>' feature.
      (second UPDATE in both procedures always uses PK of view v_test2)

    We make several calls of each procedure (see N_MEASURES) and do invocation of psutil.Process(fb_pid).cpu_times()
    before and after each call, thus and obtain CPU User Time values for each iteration.
    Difference between them can be considered as much more accurate performance estimation.

    ::: NB :::
    It was decided NOT to analyze table statistics (via trace or get_table_access_stats() method of db_conn)
    because access paths can change in the future. Particularly, if HASH will be used for joining indexed columns
    and optimizer will choose it for one of statements then statistics can have big numbers of natural reads for
    if and still no nat. reads in another statement.

    Checked on Windows:
        3.0.1.32609 (27-sep-2016, before fix):
        ratio_lst: [222.0, 110.5, 220.0, 220.0, 111.0, 222.0, 109.5, 123.5, 148.0, 133.0, 240.0, 228.0, 224.0, 221.0, 219.0, 217.0, 222.0, 222.0, 221.0, 222.0]
        median_ratio= 220.5

        3.0.2.32703 (21-mar-2017, after fix):
        ratio_lst: [0.73, 0.73, 0.8, 0.68, 0.73, 0.8, 0.68, 0.94, 0.82, 0.75, 0.66, 0.75, 0.62, 0.65, 0.57, 0.8, 0.86, 0.63, 0.52, 0.72]
        median_ratio= 0.73

        recent versions (3.0.8.33535, 4.0.1.2660, 5.0.0.311):
        ratio_lst: [2.0, 0.5, 1.0, 2.0, 0.5, 1.0, 15625.0, 0.5, 1.0, 0.5, 0.5, 1.0, 3.2e-05, 1.0, 1.0, 0.5, 2.0, 0.5, 1.0, 1.0]
        median_ratio= 1.0

        50sS, build 5.0.0.311 : 2.814s ;   50Cs, build 5.0.0.311 : 3.153s.
        40sS, build 4.0.1.2660 : 2.818s ;  40Cs, build 4.0.1.2660 : 3.532s.
        30sS, build 3.0.8.33535 : 2.548s ; 30Cs, build 3.0.8.33535 : 3.162s.

    21.11.2021. Checked on Linux (after installing pakage psutil):
        5.0.0.313 SS:   3.264s
        4.0.1.2668 SS:  2.968s
        3.0.8.33540 SS: 3.285s

    OLD COMMENTS:
    -------------
    Confirmed trouble on WI-V3.0.2.3262, ratio was ~146(!).
    Performance is OK  on WI-V3.0.2.32629 and WI-T4.0.0.450
    Plans differ on 3.0.2 and 4.0 thus they are excluded from the output!
    Performance of 4.0 significantly _WORSE_ than of 3.0.2, sent letter to dimitr, 11.11.2016 13:47.
JIRA:        CORE-5393
FBTEST:      bugs.core_5393
[25.07.2022] pzotov
  Checked on 3.0.8.33535, 4.0.1.2692, 5.0.0.591
"""

import psutil
import pytest
from firebird.qa import *

#--------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------

###########################
###   S E T T I N G S   ###
###########################

# Number of PSQL calls
#
N_MEASURES = 21

# How many rows must be inserted in the table.
# DO NOT set this value less then 20'000, otherwise ratios will be unreal.
#
N_COUNT_PER_MEASURE = 50000

# Maximal value for MEDIAN of ratios between CPU user time when comparison was made.
# Results of 25.07.2022: 3.0.8.33535: ~0.73;  4.0.1.2692: ~0.73; 5.0.0.591: ~0.84
#
MEDIAN_TIME_MAX_RATIO = 1.10
############################

init_script = '''
    set bail on;
    set echo on;
    set term ^;
    recreate global temporary table test (id int primary key using index test_pk, col int) on commit delete rows
    --recreate table test (id int primary key using index test_pk, col int)
    ^
    -- view1, must contain a subquery:
    create or alter view v_test1 (id1, id2, col1, col2, dummy) as
    select t1.id, t2.id, t1.col, t2.col, (select 1 from rdb$database)
    from test t1
    join test t2 on t1.col = t2.id
    ^

    -- view2, must contain a subquery:
    create or alter view v_test2 (id1, id2, col1, col2, dummy) as
    select ta.id, tb.id, ta.col, tb.col, (select 1 from rdb$database)
    from test ta
    join test tb on ta.col = tb.id
    ^

    -- empty triggers to make both views updatable:
    create or alter trigger trg_vtest1_bu for v_test1 before update as
    begin
    end
    ^
    create or alter trigger trg_vtest2_bu for v_test2 before update as
    begin
    end
    ^

    create procedure sp_insert_rows( a_rows_cnt int ) as
        declare i int = 0;
    begin
        while ( i < a_rows_cnt ) do
        begin
            insert into test(id, col) values(:i, :i);
            i = i + 1;
        end
    end
    ^

    create procedure sp_upd_using_primary_key_field as
    begin

        for select id1 from v_test1 as cursor c do
        begin
            -- ####################################
            -- "Both updates utilize the primary key index for table T1. So far so good."
            -- ####################################
            update v_test1 x set x.col1 = 1
            where x.id1 = c.id1; ------------------ u s i n g   t a b l e s    P K

            update v_test2 y set y.col1 = 1
            where y.id1 = c.id1;

            -- execute statement ('update v_test2 set col1 = 1 where id1 = ?') (c.id1);

        end
    end
    ^

    create procedure sp_upd_using_current_of_cursor as
    begin
      for select id1 from v_test1 as cursor c do
      begin

        -- first update is not reported in the plan because it's based on the same cursor as the select itself
        update v_test1 u set u.col1 = 1
        where current of c; ------------------ u s i n g    c u r r e n t   o f

        -- ######################################################
        -- "... second update is unable to utilize the primary key index for table T1 anymore"
        -- ######################################################
        -- In the production database, this issue is causing 100x degradation in execution time.
        update v_test2 v set v.col1 = 1
        where v.id1 = c.id1;

        -- execute statement ('update v_test2 set col1 = 1 where id1 = ?') (c.id1);

      end
    end
    ^
    set term ;^
    commit;
'''

db = db_factory(init = init_script)

act = python_act('db')

MSG_PREFIX = 'Median value of elapsed time ratios: '
expected_stdout = f"""
    {MSG_PREFIX}acceptable.
"""

@pytest.mark.perf_measure
@pytest.mark.version('>=3.0.2')
def test_1(act: Action, capsys):

    with act.db.connect() as con:

        cur=con.cursor()

        cur.execute("select rdb$relation_id from rdb$relations where rdb$relation_name = upper('test')")
        test_rel_id = int(cur.fetchone()[0])

        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])

        name_suffixes = ( 'using_primary_key_field', 'using_current_of_cursor' )
        sp_time = {}

        for i in range(0, N_MEASURES):
            cur.callproc( 'sp_insert_rows', (N_COUNT_PER_MEASURE,) )
            for x_name in name_suffixes:

                tabstat1 = [ p for p in con.info.get_table_access_stats() if p.table_id == test_rel_id ]

                fb_info_init = psutil.Process(fb_pid).cpu_times()
                cur.callproc( 'sp_upd_%(x_name)s' % locals() )
                fb_info_curr = psutil.Process(fb_pid).cpu_times()

                tabstat2 = [ p for p in con.info.get_table_access_stats() if p.table_id == test_rel_id ]

                # [ ... 'backouts', 'deletes', 'expunges', 'indexed', 'inserts', 'purges', 'sequential', 'table_id', 'table_name', 'updates']
                #print('mode:',x_name)
                #print('stat before:')
                #for t in tabstat1:
                #   print( t.table_id, t.sequential, t.indexed )
                #print('stat after:')
                #for t in tabstat2:
                #   print( t.table_id, t.sequential, t.indexed )

                sp_time[ x_name, i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)

            # commit will truncate GTT, so we can insert again into empty table on next iteration:
            con.commit()


    ratio_lst = []
    for i in range(0, N_MEASURES):
        ratio_lst.append( sp_time['using_current_of_cursor',i]  / sp_time['using_primary_key_field',i]  )

    median_ratio = median(ratio_lst)

    #for k,v in sorted(sp_time.items()):
    #    print(k,':::',v)
    #print('ratio_lst:', [round(p,2) for p in ratio_lst] )
    #print('median_ratio=',median_ratio)

    msg = MSG_PREFIX
    if median_ratio < MEDIAN_TIME_MAX_RATIO:
        msg += 'acceptable.'
        print(msg)
    else:
        msg += '/* perf_issue_tag */ INACCEPTABLE: %12.2f -- more than threshold = %5.2f' % (median_ratio, MEDIAN_TIME_MAX_RATIO)
        print(msg)

        print('Check values for %d measurements:' % N_MEASURES)
        for i,p in enumerate(ratio_lst):
            print( '%3d : %12.2f' % (i,p) )

        #print('Check values of elapsed time:')
        #for k,v in sorted(sp_time.items()):
        #    print('proc_name, measure: ',k,' - elapsed time, ms: ',v)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
