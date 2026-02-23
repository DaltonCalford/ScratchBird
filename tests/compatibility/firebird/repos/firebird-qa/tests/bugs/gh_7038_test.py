#coding:utf-8

"""
ID:          issue-7038
ISSUE:       7038
TITLE:       Improve performance of STARTING WITH with insensitive collations
DESCRIPTION:
  21.11.2021. Totally re-implemented, package 'psutil' must be installed.

  We make two calls of psutil.Process(fb_pid).cpu_times() (before and after SQL code) and obtain CPU User Time
  values from each result.
  Difference between them can be considered as much more accurate performance estimation.

  We make <N_MEASURES> calls of two stored procedures with names: 'sp_ptbr_test' and 'sp_utf8_test'.
  Each procedure has input argument which is required number of iterations with evaluation result of STARTING WITH.
  Number of iterations it set in variable N_COUNT_PER_MEASURE.

  Each result (difference between cpu_times().user values when apropriate procedure finishes) is added to the list.
  Finally, we evaluate MEDIAN of ration between cpu user time which was received for SIMILAR_TO and LIKE statements.
  If this median is less then threshold (see var. UTF8_TO_PTBR_MAX_RATIO) then result can be considered as ACCEPTABLE.

  See also: https://psutil.readthedocs.io/en/latest/#psutil.cpu_times

  Confirmed poor perfromance on 5.0.0.279: ratio median is about 1.8.
  Checked on 5.0.0.298 (intermediate build, date: 05.11.2021 13:10) - performance is better, ratio is about 1.35.
    Example of data (for 15 calls):
    [ 1.36 ,1.36 ,1.33 ,1.35 ,1.28 ,1.36 ,1.36 ,1.37 ,1.36 ,1.33 ,1.30 ,1.79 ,1.31 ,1.30 ,1.30 ]

  21.11.2021. Checked on Linux (after installing pakage psutil):
    5.0.0.313 SS: 17.076s
    5.0.0.313 CS: median = 1.51163, data:  1.56, 1.40, 1.52, 1.50, 1.63, 1.53, 1.52, 1.28, 1.40, 1.75, 1.45, 1.43, 1.51, 1.48, 1.52.
FBTEST:      bugs.gh_7038
NOTES:
    [21.07.2022] pzotov
    Checked on 5.0.0.591
"""
import os
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
# How many times we call PSQL code (two stored procedures:
# one for performing comparisons based on LIKE, second based on SIMILAR TO statements):
N_MEASURES = 21

# How many iterations must be done in each of stored procedures when they work:
N_COUNT_PER_MEASURE = 1000000

# Maximal value for MEDIAN of ratios between CPU user time when comparison was made.
#

UTF8_TO_PTBR_MAX_RATIO = 1.45 if os.name == 'nt' else 1.85
#############################

init_script = \
'''
set term ^;
create or alter procedure sp_ptbr_test (
        n_count int
    ) as
        declare p varchar(1) character set win1252 collate win_ptbr = 'x';
        declare s varchar(60) character set win1252 collate win_ptbr = 'x12345678901234567890123456789012345678901234567890123456789';
        declare b boolean;
    begin
        while (n_count > 0)
        do
        begin
            b = s starting with p;
            n_count = n_count - 1;
        end
    end
^
create or alter procedure sp_utf8_test (
    n_count int
) as
    declare p varchar(1) character set utf8 collate unicode_ci = 'x';
    declare s varchar(60) character set utf8 collate unicode_ci = 'x12345678901234567890123456789012345678901234567890123456789';
    declare b boolean;
begin
    while (n_count > 0)
    do
    begin
        b = s starting with p;
        n_count = n_count - 1;
    end
end
^
commit
^
'''

db = db_factory( init = init_script )
act = python_act('db')

expected_stdout = """
    Duration ratio: acceptable
"""

@pytest.mark.perf_measure
@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):
    
    with act.db.connect() as con:
        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')

        fb_pid = int(cur.fetchone()[0])
        sp_time = {}
        for i in range(0, N_MEASURES):
            for x_charset in ('ptbr', 'utf8'):
                fb_info_init = psutil.Process(fb_pid).cpu_times()
                cur.callproc('sp_' + x_charset + '_test', (N_COUNT_PER_MEASURE,) )
                fb_info_curr = psutil.Process(fb_pid).cpu_times()
                sp_time[ x_charset, i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)
             #print( 'String form: "%s", median ratio: %s' % ( x_charset, 'acceptable' if median(ratio_list) <= UTF8_TO_PTBR_MAX_RATIO else 'TOO BIG: ' + str(median(ratio_list))  ) )

    ratio_lst = []
    for i in range(0, N_MEASURES):
        ratio_lst.append( sp_time['utf8',i]  / sp_time['ptbr',i]  )
    median_ratio = median(ratio_lst)
    print( 'Duration ratio: ' + ('acceptable' if median_ratio < UTF8_TO_PTBR_MAX_RATIO else '/* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(UTF8_TO_PTBR_MAX_RATIO) ) ) )

    if median_ratio >= UTF8_TO_PTBR_MAX_RATIO:
        print('Ratio statistics for %d measurements' % N_MEASURES)
        for p in ratio_lst:
            print( '%12.2f' % p )


    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
