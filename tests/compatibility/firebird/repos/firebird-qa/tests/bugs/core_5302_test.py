#coding:utf-8

"""
ID:          issue-2047
ISSUE:       2047
TITLE:       Performance regression when bulk inserting into table with indices
DESCRIPTION:
    21.11.2021. Totally re-implemented, package 'psutil' must be installed.

    We make two calls of psutil.Process(fb_pid).cpu_times() (before and after SQL code) and obtain CPU User Time
    values from each result.
    Difference between them can be considered as much more accurate performance estimation.

    Test creates two tables: 'test_non_idx' and 'test_indexed' (second of them with several indices).
    Then we create two stored procedures ('sp_non_idx' and 'sp_indexed') which perform INSERTs into these tables.
    Number of INSERTs it set by variable N_COUNT_PER_MEASURE.

    Each of these procedures is called <N_MEASURES> times, with saving CPU 'user_time' that OS dedicated to FB process
    to complete its job (we use psutil package to obtain this value).

    After each call we drop procedures, recreate tables again and create procedures in order to repeat DML.

    Each result (difference between cpu_times().user values when apropriate procedure finishes) is added to the list.

    Finally, we evaluate MEDIAN of ratios between cpu user time which was received for INDEXED and NON_INDEXED tables.
    Obviously, time for indexed table must be greater than for non-indexed one.

    If ratios median is less then threshold (see var. INSERTS_TIME_MAX_RATIO) then result can be considered as ACCEPTABLE.
    Otherwise we show ratios median and list of their values for each measurement.

    See also:
    https://psutil.readthedocs.io/en/latest/#psutil.cpu_times

    Result for FB _before_ fix:
    3.0.0.32483 (built 15-apr-2016):
          38    27      49      26      27      30      26      27      46      28      50      26      46      26      27      26      26      26      48      48 // median = 26.875; max = 50

    Result for FB _after_ fix:
    3.0.1.32609 (built 27-sep-2016):
          10    11      10      10      11      10      9       10      11      9       14      11      10      10      13      10      10      11      10      10 // median = 10; max = 13

    All builds of nov-2021 (3.0, 4.0 and 5.0; checked SS/CS) have similar result to this:
        build        mode   median   max
        3.0.8.33446    CS     14      17
        3.0.8.33527    SS     14      16
        4.0.0.2422     CS     13      14
        4.0.1.2660     SS     12      15
        5.0.0.311      SS     14      17
        5.0.0.309      CS     12      16

    Reasonable value of threshold can be equal to MAXIMAL of these values plus 2..3.
    Checked on: 5.0.0.311, 5.0.0.309; 4.0.1.2660, 4.0.0.2422; 3.0.8.33527, 3.0.8.33446 (all: SS/CS)

    ############################################################################################

    Old comments (before 16.11.2021):

    _BEFORE_ this ticket was fixed ratio was following:
        Ratio for 4.0.0.258: ~32...34 -- poor
        Ratio for 3.0.1.32566: ~23...24 -- poor

    _AFTER_ ticket was fixed ratio is:
        Ratio for 4.0.0.313:   ~11...13 -- OK
        Ratio for 3.0.1.32568: ~10...11 -- OK

    21.11.2021. Checked on Linux (after installing pakage psutil):
        5.0.0.313 SS:   94.000s
        4.0.1.2668 SS:  87.167s
        3.0.8.33540 SS: 86.931s

    Fix for 4.0 was 07-jul-2016, see here:
    https://github.com/FirebirdSQL/firebird/commit/a75e0af175ea6e803101b5fd62ec91cdf039b951
    Fix for 3.0 was 27-jul-2016, see here:
    https://github.com/FirebirdSQL/firebird/commit/96a24228b61003e72c68596faf3c4c4ed0b95ea1
JIRA:        CORE-5302
FBTEST:      bugs.core_5302
[25.07.2022] pzotov
  Checked on 3.0.8.33535, 4.0.1.2692, 5.0.0.591

"""

import psutil
import pytest
from firebird.qa import *

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

###########################
###   S E T T I N G S   ###
###########################

# Number of PSQL calls:
#
N_MEASURES = 21

# How many iterations must be done in each of stored procedures when they work.
# DO NOT set this value less then 20'000, otherwise ratios will be unreal.
#
N_COUNT_PER_MEASURE = 50000

# Maximal value for MEDIAN of ratios between CPU user time when comparison was made.
# Results of 25.07.2022: 3.0.8.33535: ~12;  4.0.1.2692: ~9.5; 5.0.0.591: ~13
#
INSERTS_TIME_MAX_RATIO = 20
#############################

db = db_factory()

act = python_act('db')

MSG_PREFIX = 'CPU time ratio for Indexed vs NON-indexed inserts: '
expected_stdout = f"""
{MSG_PREFIX}acceptable.
"""

@pytest.mark.perf_measure
@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    sel_tables_ddl='''
        recreate table test_non_idx(x int, y int, z int)^
        recreate table test_indexed(x int, y int, z int)^
        create index test_indexed_x on test_indexed(x)^
        create index test_indexed_y on test_indexed(y)^
        create index test_indexed_z on test_indexed(z)^
        create index test_indexed_xy on test_indexed(x, y)^
        create index test_indexed_xz on test_indexed(x, z)^
        create index test_indexed_xyz on test_indexed(x, y, z)^
    '''

    sp_make_proc_ddl='''
        create or alter procedure sp_%(x_name)s ( n_count int = 50000) as
        begin
            while (n_count > 0) do
            begin
                insert into test_%(x_name)s (x, y, z) values(:n_count, :n_count, :n_count);
                n_count = n_count - 1;
            end
        end
    '''

    sp_drop_proc_ddl='''
        create or alter procedure sp_%(x_name)s ( n_count int = 50000) as begin end
    '''
   
    name_suffixes = ( 'non_idx', 'indexed' )

    # --------------------------------------

    sp_time = {}
    with act.db.connect() as con:
        cur = con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])
        for i in range(0, N_MEASURES):
            
            for x_name in name_suffixes:
                #--- recreate procs with empty bodies ---
                cur.execute( sp_drop_proc_ddl % locals() )

            for x in sel_tables_ddl.split('^'):
                #--- recreate tables ---
                if x.strip():
                    cur.execute( x.strip() )

            for x_name in name_suffixes:
                #--- recreate procs with normal bodies ---
                cur.execute( sp_make_proc_ddl % locals() )
            con.commit()
            
            for x_name in name_suffixes:
                fb_info_init = psutil.Process(fb_pid).cpu_times()
                cur.callproc( 'sp_%(x_name)s' % locals(), (N_COUNT_PER_MEASURE,) )
                fb_info_curr = psutil.Process(fb_pid).cpu_times()

                sp_time[ x_name, i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)
            con.commit()


        # --------------------------------------

    ratio_lst = []
    for i in range(0, N_MEASURES):
        ratio_lst.append( sp_time['indexed',i]  / sp_time['non_idx',i]  )

    median_ratio = median(ratio_lst)

    print( MSG_PREFIX + ('acceptable.' if median_ratio < INSERTS_TIME_MAX_RATIO else '/* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(INSERTS_TIME_MAX_RATIO) ) ) )
    if median_ratio >= INSERTS_TIME_MAX_RATIO:
        print('Ratio statistics for %d measurements' % N_MEASURES)
        for p in ratio_lst:
            print( '%12.2f' % p )


    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
