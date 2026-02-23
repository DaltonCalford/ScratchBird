#coding:utf-8

"""
ID:          issue-7759
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7759
TITLE:       Routine calling overhead increased by factor 6
DESCRIPTION:
    Test creates standalone function and procedure ('sa_func', 'sa_proc').
    Also, package 'pg_test' with similar function and procedure is created ('pg_func', 'pg_proc').
    Every of these units is called from appropriate procedures which accept input argument for number
    of iterations in the loop that must be performed ('sa_func_caller', 'sa_proc_caller' etc).
    Also, there is procedure 'sp_dummy_loop' which does only loop with empty body, i.e. has no calls.
    This SP, of course, will be performed for minimal time comparing to all other unit.

    Number of iteration in each loop is defined by N_COUNT_PER_MEASURE.

    Duration is measured as difference between psutil.Process(fb_pid).cpu_times() counters.
    We do these measures <N_MEASURES> times for each SP, and each result is added to the list
    which, in turn, is the source for median evaluation.

    Finally, we get ratio between minimal and maximal medians (see 'median_ratio')
    On Windows 8.1 usually this ratio is no more than 8 (before fix it was in the scope 22...33).
    
    Test is considered as passed if median_ratio less than threshold <MAX_RATIO>.
NOTES:
    [23.11.2023] pzotov
    Fix in 4.x: ee357c9e6f081d3fd58e99aed99d15ec6476cedb (26-sep-2023)
    Fix in 5.x: d621ffbe0cf2d43e13480628992180c28a5044fe (03-oct-2023; since build 5.0.0.1237).

    Following is median ratio values for misc snapshots:
    1. Before fix:
        5.0.0.1236 : 33.50 33.50 22.00 22.00 33.00 33.00 22.00 22.00 32.50 22.00
    2. After fix:
        4.0.5.3031 :  7.00  7.00  4.67  7.00  4.67  7.50  7.00  7.50  7.00  7.00
        5.0.0.1277 :  5.33  5.33  5.33  5.33  8.00  5.33  5.33  5.33  5.33  5.33
        6.0.0.139  :  7.50  8.00  8.00  5.33  8.00  8.00  8.00  8.00  8.00  8.00

    Checked on 6.0.0.139, 5.0.0.1277, 4.0.5.3031.
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
MAX_RATIO = 12
##############

init_script = \
f'''
    set term ^;
    create or alter function sa_func returns varchar(10) as
    begin
        return '';
    end
    ^
    -----------------------------------------------------------
    create or alter procedure sa_proc returns (dummy_txt varchar(10)) as
    begin
        dummy_txt = '';
        suspend;
    end
    ^
    -----------------------------------------------------------
    create or alter package pg_test as
    begin
        function pg_func returns varchar(10);
        procedure pg_proc returns( dummy_txt varchar(10) );
        procedure pg_func_caller(iter_cnt int);
        procedure pg_proc_caller(iter_cnt int);
    end
    ^
    recreate package body pg_test as
    begin
        function pg_func returns varchar(10) as
        begin
            return '';
        end

        procedure pg_proc returns( dummy_txt varchar(10)) as
        begin
            dummy_txt = '';
            suspend;
        end

        procedure pg_func_caller(iter_cnt int) as
            declare i int = 0;
            declare dummy_txt varchar(10);
        begin
            while (i < iter_cnt) do
            begin
                dummy_txt = pg_func();
                i = i + 1;
            end  
        end

        procedure pg_proc_caller(iter_cnt int) as
            declare i int = 0;
            declare dummy_txt varchar(10);
        begin
            while (i < iter_cnt) do
            begin
                execute procedure pg_proc returning_values dummy_txt;
                i = i + 1;
            end  
        end
    
    end
    ^
    -----------------------------------------------------------
    create procedure sp_dummy_loop(iter_cnt int) as 
        declare i int = 0;
    begin
        while (i < iter_cnt) do
        begin
            i = i + 1;
        end  
    end
    ^
    -----------------------------------------------------------
    create procedure sa_func_caller(iter_cnt int) as 
        declare i int = 0;
        declare dummy_txt varchar(10);
    begin
        while (i < iter_cnt) do
        begin
            dummy_txt = sa_func();
            i = i + 1;
        end  
    end
    ^
    -----------------------------------------------------------
    create procedure sa_proc_caller(iter_cnt int) as 
        declare i int = 0;
        declare dummy_txt varchar(10);
    begin
        while (i < iter_cnt) do
        begin
            execute procedure sa_proc returning_values dummy_txt;
            i = i + 1;
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
@pytest.mark.version('>=4.0.4')
def test_1(act: Action, capsys):
    
    with act.db.connect() as con:
        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])

        sp_time = {}
        for i in range(0, N_MEASURES):
            for sp_name in ('sp_dummy_loop', 'sa_func_caller', 'sa_proc_caller', 'pg_test.pg_func_caller', 'pg_test.pg_proc_caller', ):
                fb_info_init = psutil.Process(fb_pid).cpu_times()
                cur.callproc( sp_name, (N_COUNT_PER_MEASURE,) )
                fb_info_curr = psutil.Process(fb_pid).cpu_times()
                sp_time[ sp_name, i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)


    sp_dummy_loop_median = median([v for k,v in sp_time.items() if k[0] == 'sp_dummy_loop'])
    sa_func_caller_median = median([v for k,v in sp_time.items() if k[0] == 'sa_func_caller'])
    sa_proc_caller_median = median([v for k,v in sp_time.items() if k[0] == 'sa_proc_caller'])
    pg_func_caller_median = median([v for k,v in sp_time.items() if k[0] == 'pg_test.pg_func_caller'])
    pg_proc_caller_median = median([v for k,v in sp_time.items() if k[0] == 'pg_test.pg_proc_caller'])

    max_median = max(sa_func_caller_median, sa_proc_caller_median, pg_func_caller_median, pg_proc_caller_median)
    min_median = max(sp_dummy_loop_median, 0.000001)
    median_ratio = max_median / min_median

    print( 'Medians ratio: ' + ('acceptable' if median_ratio <= MAX_RATIO else '/* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(MAX_RATIO) ) ) )
    if median_ratio > MAX_RATIO:
        print('CPU times for each of {N_MEASURES} measures:')
        for k,v in sp_time.items():
            print(k,':::',v)
        print(f'Median cpu time for {N_MEASURES} measures using loops for {N_COUNT_PER_MEASURE} iterations in each SP call:')
        print('sp_dummy_loop_median:',sp_dummy_loop_median)
        print('sa_func_caller_median:',sa_func_caller_median)
        print('sa_proc_caller_median:',sa_proc_caller_median)
        print('pg_func_caller_median:',pg_func_caller_median)
        print('pg_proc_caller_median:',pg_proc_caller_median)
        print('max_median:', max_median)
        print('min_median:', min_median)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
