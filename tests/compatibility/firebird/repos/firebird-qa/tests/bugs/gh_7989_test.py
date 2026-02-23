#coding:utf-8

"""
ID:          issue-7989
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7989
TITLE:       Improve performance of external (UDR) functions
DESCRIPTION:
    We can estimate perfomance by comparison of time that is spent to call UDR vs result of some crypt function.
    Function crypt_hash(<string> using SHA512) has been selected for that because of notable CPU consumation.
    Stored procedured SP_GEN_HASH is created for evaluation of crypt hash, it will run loop for N_HASH_EVALUATE_COUNT times.
    Duration for each measure is difference between psutil.Process(fb_pid).cpu_times() counters.

    We do <N_MEASURES> to call SP vs UDR, with adding results to map.
    UDR that determines whether date relates to LEAP year is used for this test.
    It is defined as 'udf_compat!UC_isLeapYear' in UDR engine, see $FB_HOME/upgrade/v4.0/udf_replace.sql 

    Finally, we get ratio between medians of these measures (see 'median_ratio')
    Test is considered as passed if median_ratio less than threshold <MAX_RATIO>.
NOTES:
    [18.05.2025] pzotov.
    Initial commit that introduces inmprovement: 29.02.2024 00:43
        https://github.com/FirebirdSQL/firebird/commit/547cb8388b9c72a329ed9bfe8c25f8ee696a112e
    Postfix for #7989 - Improve performance of external (UDR) functions: 17.03.2024 04:14
        https://github.com/FirebirdSQL/firebird/commit/e4377213b4c9767cafe92502797db4f040a9e6be

    1. Medians ratio on Windows:
        * before: 0.46
        * after:  0.31
    2. Test DB must NOT have charset = utf8, otherwise 'implementation limit exceeded' will raise; 'ascii' charset was selected for work.
    3. Just for info: isLeapYear() can be implemented using pure PSQL:
           create or alter function isLeapPSQL (a_year int) returns boolean as
           begin
               return bin_and( (a_year * 1073750999), 3221352463) <= 126976;
           end
       Benchmark shows that this code *was* faster than UDR before #547cb838 but now it runs slower (with raio ~ 2450/3050 ms).
       See:
           [EN]: https://hueffner.de/falk/blog/a-leap-year-check-in-three-instructions.html
           [RU]: https://habr.com/ru/articles/910188/

    Checked on 6.0.0.269; 6.0.0.273; 6.0.0.783.
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

# How many times we do measure:
N_MEASURES = 21

N_UDR_CALLS_COUNT = 50000

# How many iterations must be done for hash evaluation:
N_HASH_EVALUATE_COUNT = 3000

# Maximal value for ratio between maximal and minimal medians
#
################
MAX_RATIO = 0.40
################

init_script = \
f'''
    set term ^;
    create or alter procedure sp_gen_hash (n_cnt int) as
        declare v_hash varbinary(64);
        declare s varchar(32765);
    begin
        s = lpad('', 32765, uuid_to_char(gen_uuid()));
        while (n_cnt > 0) do
        begin
            v_hash = crypt_hash(s using SHA512);
            n_cnt = n_cnt - 1;
        end
    end
    ^

    -- from $FB_HOME/upgrade/v4.0/udf_replace.sql:
    create or alter function isLeapUDR (a_timestamp timestamp) returns boolean
        external name 'udf_compat!UC_isLeapYear'
        engine udr;
    ^
    create or alter procedure sp_udr_call (n_cnt int) as
        declare a_date date;
        declare b boolean;
    begin
        a_date = current_date;
        while (n_cnt > 0) do
        begin
            b = isLeapUDR(a_date);
            n_cnt = n_cnt - 1;
        end
    end
    ^
    commit
    ^
'''

db = db_factory(init = init_script, charset = 'ascii')
act = python_act('db')

expected_stdout = """
    Medians ratio: acceptable
"""

@pytest.mark.perf_measure
@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    with act.db.connect() as con:
        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])

        times_map = {}
        for i in range(0, N_MEASURES):
            fb_info_init = psutil.Process(fb_pid).cpu_times()
            cur.callproc( 'sp_gen_hash', (N_HASH_EVALUATE_COUNT,) )
            fb_info_curr = psutil.Process(fb_pid).cpu_times()
            times_map[ 'hash_eval', i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)

            fb_info_init = psutil.Process(fb_pid).cpu_times()
            cur.callproc( 'sp_udr_call', (N_UDR_CALLS_COUNT,) )
            fb_info_curr = psutil.Process(fb_pid).cpu_times()
            times_map[ 'udr_call', i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)


    sp_gen_hash_median = median([v for k,v in times_map.items() if k[0] == 'hash_eval'])
    sp_udr_call_median = median([v for k,v in times_map.items() if k[0] == 'udr_call'])

    median_ratio = sp_udr_call_median / sp_gen_hash_median

    print( 'Medians ratio: ' + ('acceptable' if median_ratio <= MAX_RATIO else '/* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(MAX_RATIO) ) ) )
    if median_ratio > MAX_RATIO:
        print(f'CPU times for each of {N_MEASURES} measures:')
        for sp_name in ('hash_eval', 'udr_call', ):
            print(f'{sp_name=}:')
            for p in [v for k,v in times_map.items() if k[0] == sp_name]:
                print(p)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
