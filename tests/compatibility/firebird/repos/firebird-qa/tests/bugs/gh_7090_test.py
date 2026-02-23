#coding:utf-8

"""
ID:          issue-7092
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7092
TITLE:       Performance degradation with CURRENT_DATE, LOCALTIME and LOCALTIMESTAMP
DESCRIPTION:
    Test uses two procedures:
        * one with loop for integer values that does nothing more, and
        * second with same loop but with three statements from ticket (on every iteration).

    Duration is measured as difference between psutil.Process(fb_pid).cpu_times() counters.
    We do these measures <N_MEASURES> times for each SP, and each result is added to the list
    which, in turn, is the source for median evaluation.
    Finally, we get ratio between minimal and maximal medians (see 'median_ratio')
    On Windows 8.1 usually this ratio is about 1.67 (before fix it was more than 50).
    
    Test is considered as passed if median_ratio less than threshold <MAX_RATIO>.
NOTES:
    Confirmed problem on:
        4.0.1.2699 (01-jan-2022): median ratio was 50 ... 51 (17.01 vs 0.34)
        5.0.0.362  (01-jan-2022): median ratio was 51 ... 55 (19.91 vs 0.36)
    Checked on 6.0.0.195, 5.0.0.1305, 4.0.5.3049.
    Scope of median ratio values: 1.65 ... 1.77
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

# How many times we call procedures:
N_MEASURES = 15

# How many iterations must be done:
N_COUNT_PER_MEASURE = 1000000

# Maximal value for ratio between maximal and minimal medians
#
MAX_RATIO = 4
##############

init_script = \
f'''
    set term ^;
    create procedure sp_check_loop(a_limit int)
    as
        declare v_current_date date;
        declare v_localtime time;
        declare v_localtimestamp timestamp;
        declare n int = 1;
    begin
        while (n < a_limit) do
        begin
            n = n + 1;
            v_current_date = current_date;
            v_localtime = localtime;
            v_localtimestamp = localtimestamp;
        end
    end
    ^
    create procedure sp_dummy_loop(a_limit int)
    as
        declare n int = 1;
    begin
        while (n < a_limit) do
        begin
            n = n + 1;
        end
    end
    ^
    commit
    ^
'''

db = db_factory(init = init_script)
act = python_act('db')

expected_stdout = """
    Medians ratio: acceptable
"""

@pytest.mark.perf_measure
@pytest.mark.version('>=4.0.2')
def test_1(act: Action, capsys):
    
    with act.db.connect() as con:
        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])

        sp_time = {}
        for i in range(0, N_MEASURES):
            for sp_name in ('sp_check_loop', 'sp_dummy_loop'):
                fb_info_init = psutil.Process(fb_pid).cpu_times()
                cur.callproc( sp_name, (N_COUNT_PER_MEASURE,) )
                fb_info_curr = psutil.Process(fb_pid).cpu_times()
                sp_time[ sp_name, i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)


    sp_ctime_median = median([v for k,v in sp_time.items() if k[0] == 'sp_check_loop'])
    sp_dummy_median = median([v for k,v in sp_time.items() if k[0] == 'sp_dummy_loop'])
    #----------------------------------
    median_ratio = sp_ctime_median / sp_dummy_median

    print( 'Medians ratio: ' + ('acceptable' if median_ratio <= MAX_RATIO else '/* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(MAX_RATIO) ) ) )
    if median_ratio > MAX_RATIO:
        print('CPU times for each of {N_MEASURES} measures:')
        for k,v in sp_time.items():
            print(k,':::',v)
        print(f'Median cpu time for {N_MEASURES} measures using loops for {N_COUNT_PER_MEASURE} iterations in each SP call:')
        print('sp_ctime_median:',sp_ctime_median)
        print('sp_dummy_median:',sp_dummy_median)
        print('median_ratio:',median_ratio)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
