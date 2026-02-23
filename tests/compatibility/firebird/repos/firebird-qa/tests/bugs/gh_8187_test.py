#coding:utf-8

"""
ID:          issue-8187
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8187
TITLE:       Performance regression in generating of UUID values after introducing GUID-v7
DESCRIPTION:
    We can estimate perfomance by comparison of time that is spent to generate UUIDs vs result of some crypt function
    (commit: https://github.com/FirebirdSQL/firebird/commit/43e40886856ace39e0b3a1de7b00c53325e67225).
    Function crypt_hash(<string> using SHA512) has been selected for that.
    Two procedures are created for generating appropriate results, SP_GEN_UUID and SP_GEN_HASH.
    Duration for each of them is measured as difference between psutil.Process(fb_pid).cpu_times() counters.
    We do these measures <N_MEASURES> times for each SP, and each result is added to the list
    which, in turn, is the source for median evaluation.
    Finally, we get ratio between minimal and maximal medians (see 'median_ratio')
    
    Test is considered as passed if median_ratio less than threshold <MAX_RATIO>.
NOTES:
    [05.08.2024] pzotov.
    It seems that SP_CRYPT_HASH consumes lot of CPU resources thus it must be called much less times than SP_GEN_UUID,
    see settings N_COUNT_PER_MEASURE_HASH vs N_COUNT_PER_MEASURE_GUID.
    On Windows 10 usually ratio between cpu_times() medians of these two SPs is about 0.5 (before fix it was about 30).

    Confirmed bug on 6.0.0.405.
    Checked on Windows, 6.0.0.406, 5.0.1.1469, 4.0.5.3139 (SS and CS).

    [08.08.2024] pzotov
    Separated definition of MAX_RATIO value, it depends on OS.
    Linux results show that medians ratio is about 2.3 ... 2.6.
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

# How many times we call procedures:
N_MEASURES = 11

# How many iterations must be done:
N_COUNT_PER_MEASURE_HASH = 1000

N_COUNT_PER_MEASURE_GUID = 100000

# Maximal value for ratio between maximal and minimal medians
#
###########################################
MAX_RATIO = 1.0 if os.name == 'nt' else 5.5
###########################################

init_script = \
f'''
    set term ^;
    create or alter procedure sp_gen_uuid (n_cnt int) as
        declare v_guid varchar(16) character set octets;
    begin
        while (n_cnt > 0) do
        begin
            v_guid = gen_uuid();
            n_cnt = n_cnt - 1;
        end
    end
    ^
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
    commit
    ^
'''

db = db_factory(init = init_script, charset = 'win1251')
act = python_act('db')

expected_stdout = """
    Medians ratio: acceptable
"""

@pytest.mark.perf_measure
@pytest.mark.version('>=4.0.5')
def test_1(act: Action, capsys):
    
    with act.db.connect(charset = 'win1251') as con:
        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])

        sp_time = {}
        for i in range(0, N_MEASURES):
            for sp_name in ('sp_gen_hash', 'sp_gen_uuid', ):
                fb_info_init = psutil.Process(fb_pid).cpu_times()
                n_cnt = N_COUNT_PER_MEASURE_HASH if sp_name  == 'sp_gen_hash' else N_COUNT_PER_MEASURE_GUID
                cur.callproc( sp_name, (n_cnt,) )
                fb_info_curr = psutil.Process(fb_pid).cpu_times()
                sp_time[ sp_name, i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)


    sp_gen_hash_median = median([v for k,v in sp_time.items() if k[0] == 'sp_gen_hash'])
    sp_gen_uuid_median = median([v for k,v in sp_time.items() if k[0] == 'sp_gen_uuid'])
    #----------------------------------
    #print(f'{sp_gen_hash_median=}')
    #print(f'{sp_gen_uuid_median=}')

    median_ratio = sp_gen_uuid_median / sp_gen_hash_median

    print( 'Medians ratio: ' + ('acceptable' if median_ratio <= MAX_RATIO else '/* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(MAX_RATIO) ) ) )
    if median_ratio > MAX_RATIO:
        print('CPU times for each of {N_MEASURES} measures:')
        for sp_name in ('sp_gen_hash', 'sp_gen_uuid', ):
            print(f'{sp_name=}:')
            for p in [v for k,v in sp_time.items() if k[0] == sp_name]:
                print(p)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
