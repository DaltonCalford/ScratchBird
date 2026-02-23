#coding:utf-8

"""
ID:          issue-6872
ISSUE:       6872
TITLE:       Indexed STARTING WITH execution is very slow with UNICODE collation
DESCRIPTION:
    21.11.2021. Totally re-implemented, package 'psutil' must be installed.

    We make two calls of psutil.Process(fb_pid).cpu_times() (before and after SQL code) and obtain CPU User Time
    values from each result.
    Difference between them can be considered as much more accurate performance estimation.

    Test creates two tables and two procedures for measuring performance of STARTING WITH operator when it is applied
    to the table WITH or WITHOUNT unicode collation.

    On each calls of procedural code (see variable N_MEASURES) dozen execution of 'SELECT ... FROM <T> WHERE <T.C> starting with ...'
    are performed, where names '<T>' and '<T.C>' points to apropriate table and column (with or without collation).
    Number of iterations within procedures is defined by variable N_COUNT_PER_MEASURE.

    Each result (difference between cpu_times().user values when PSQL code finished) is added to the list.
    Finally, we evaluate MEDIAN of ratio values between cpu user time which was received for both of procedures.
    If this median is less then threshold (see var. ADDED_COLL_TIME_MAX_RATIO) then result can be considered as ACCEPTABLE.

    See also:
    https://psutil.readthedocs.io/en/latest/#psutil.cpu_times

    Checked on Windows:
    * builds before/without fix:
        3.0.8.33540: median = 16.30;
        4.0.1.2520:  median = 47.65
        5.0.0.85:    median = 43.14
    * builds after fix:
        4.0.1.2668:  median = 1.70
        5.0.0.313:   median = 1.80
FBTEST:      bugs.gh_6872

NOTES:
    [20.07.2022] pzotov
    Checked on 4.0.1.2692, 5.0.0.591

"""

import psutil
import pytest
from firebird.qa import *
import platform

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
N_MEASURES = 30

# How many iterations must be done in each of stored procedures when they work:
N_COUNT_PER_MEASURE = 100000

# Maximal value for MEDIAN of ratios between CPU user time when comparison was made.
#
# 05-mar-2023, debian-11, 5.0.0.970 SS: 2.0 --> 4.0
ADDED_COLL_TIME_MAX_RATIO = 4.0 if platform.system() == 'Linux' else 2.0

###############################

init_script = """
    recreate table test_utf8_miss_coll (c1 varchar(10) character set utf8);
    create index test_utf8_miss_coll_idx on test_utf8_miss_coll (c1);

    recreate table test_utf8_with_coll(c1 varchar(10) character set utf8 collate unicode);
    create index test_utf8_with_coll_idx on test_utf8_with_coll(c1);
  """

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout = """
    Duration ratio: acceptable.
"""

@pytest.mark.intl
@pytest.mark.perf_measure
@pytest.mark.version('>=4.0.1')
def test_1(act: Action, capsys):

    sp_make_proc_ddl='''
        -- x_name = 'with_coll' | 'miss_col'
        create or alter procedure sp_%(x_name)s ( n_count int = 1000000 ) as
            declare v smallint;
        begin
            while (n_count > 0) do
            begin
                select 1 from test_utf8_%(x_name)s where c1 starting with 'x' into v;
                n_count = n_count - 1;
            end
        end
    '''
    
    with act.db.connect() as con:

        name_suffixes = ( 'with_coll', 'miss_coll' )
        for x_name in name_suffixes:
            con.execute_immediate( sp_make_proc_ddl % locals() )

        con.commit()


        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])
        # --------------------------------------
        sp_time = {}

        for i in range(0, N_MEASURES):
            for x_name in name_suffixes:

                fb_info_init = psutil.Process(fb_pid).cpu_times()
                cur.callproc( 'sp_%(x_name)s' % locals(), (N_COUNT_PER_MEASURE,) )
                fb_info_curr = psutil.Process(fb_pid).cpu_times()

                sp_time[ x_name, i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)

        #---------------------------------------
        cur.close()

        ratio_lst = []
        for i in range(0, N_MEASURES):
            ratio_lst.append( sp_time['with_coll',i]  / sp_time['miss_coll',i]  )

        median_ratio = median(ratio_lst)


    if median(ratio_lst) <= ADDED_COLL_TIME_MAX_RATIO:
        print('Duration ratio: acceptable.')
    else:
        print('Duration ratio: /* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(ADDED_COLL_TIME_MAX_RATIO) ) )

        print("\\nCheck sp_time values:" )
        for k,v in sorted(sp_time.items()):
            print('k=',k,';  v=',v)

        print('\\nCheck ratio values:')
        for i,p in enumerate(ratio_lst):
            print( "%d : %12.2f" % (i,p) )
        print('\\nMedian value: %12.2f' % median_ratio)


    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

