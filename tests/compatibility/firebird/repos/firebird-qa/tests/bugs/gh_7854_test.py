#coding:utf-8

"""
ID:          issue-7854
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7854
TITLE:       Performance issue with time zones
DESCRIPTION:
    Test uses four SP with code based on example provided in the ticket:
    https://github.com/FirebirdSQL/firebird/issues/7854#issuecomment-1817956235

    Duration is measured as difference between psutil.Process(fb_pid).cpu_times() counters.
    We do these measures <N_MEASURES> times for each SP, and each result is added to the list
    which, in turn, is the source for median evaluation.
    Finally, we get ratio between minimal and maximal medians (see 'median_ratio')
    On Windows 8.1 usually this ratio is about 2.67 (before fix it was morte than 16).
    
    Test is considered as passed if median_ratio less than threshold <MAX_RATIO>.
NOTES:
    Checked on 6.0.0.139, 5.0.0.1277, 4.0.5.3031
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
N_MEASURES = 11

# How many iterations must be done:
N_COUNT_PER_MEASURE = 100000

# Maximal value for ratio between maximal and minimal medians
#
MAX_RATIO = 5
#############

init_script = \
f'''
    set term ^;
    create or alter procedure tz2ts (n_cnt int) as
        declare ts timestamp;
        declare tz timestamp with time zone;
    begin
        tz = current_timestamp;
        while (n_cnt > 0) do
        begin
            ts = tz;
            n_cnt = n_cnt - 1;
        end
    end
    ^

    create or alter procedure ts2tz (n_cnt int) as
        declare ts timestamp;
        declare tz timestamp with time zone;
    begin
        ts = localtime;
        while (n_cnt > 0) do
        begin
            tz = ts;
            n_cnt = n_cnt - 1;
        end
    end
    ^

    create or alter procedure ts2ts (n_cnt int) as
        declare ts timestamp;
        declare tx timestamp;
    begin
        ts = localtime;
        while (n_cnt > 0) do
        begin
            tx = ts;
            n_cnt = n_cnt - 1;
        end
    end
    ^

    create or alter procedure tz2tz (n_cnt int) as
        declare tz timestamp with time zone;
        declare tx timestamp with time zone;
    begin
        tz = current_timestamp;
        while (n_cnt > 0) do
        begin
            tx = tz;
            n_cnt = n_cnt - 1;
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
@pytest.mark.version('>=4.0.5')
def test_1(act: Action, capsys):
    
    with act.db.connect() as con:
        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])

        sp_time = {}
        for i in range(0, N_MEASURES):
            for sp_name in ('tz2ts', 'ts2tz', 'ts2ts', 'tz2tz'):
                fb_info_init = psutil.Process(fb_pid).cpu_times()
                cur.callproc( sp_name, (N_COUNT_PER_MEASURE,) )
                fb_info_curr = psutil.Process(fb_pid).cpu_times()
                sp_time[ sp_name, i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)


    tz2ts_median = median([v for k,v in sp_time.items() if k[0] == 'tz2ts'])
    ts2tz_median = median([v for k,v in sp_time.items() if k[0] == 'ts2tz'])
    ts2ts_median = median([v for k,v in sp_time.items() if k[0] == 'ts2ts'])
    tz2tz_median = median([v for k,v in sp_time.items() if k[0] == 'tz2tz'])
    #----------------------------------
    max_median = max(tz2ts_median, ts2tz_median, ts2ts_median, tz2tz_median)
    min_median = max(min(tz2ts_median, ts2tz_median, ts2ts_median, tz2tz_median), 0.000001)
    median_ratio = max_median / min_median

    print( 'Medians ratio: ' + ('acceptable' if median_ratio <= MAX_RATIO else '/* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(MAX_RATIO) ) ) )
    if median_ratio > MAX_RATIO:
        print('CPU times for each of {N_MEASURES} measures:')
        for k,v in sp_time.items():
            print(k,':::',v)
        print(f'Median cpu time for {N_MEASURES} measures using loops for {N_COUNT_PER_MEASURE} iterations in each SP call:')
        print('tz2ts_median:',tz2ts_median)
        print('ts2tz_median:',ts2tz_median)
        print('ts2ts_median:',ts2ts_median)
        print('tz2tz_median:',tz2tz_median)
        print('max_median:', max_median)
        print('min_median:', min_median)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
