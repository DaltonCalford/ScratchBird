#coding:utf-8

"""
ID:          issue-7382
ISSUE:       7382
TITLE:       Performance improvement for BLOB copying
DESCRIPTION:
    We create TWO stored procedures.
    Both of them create blob ("b_src") with length = <N_BLOB_FINAL_LEN>.
    First procedure creates b_src in usual way: b_src = lpad('',n_len, 'a').
    Second procedure does this in loop with incrementing blob for one character, like this: b_src = blob_append(b_src, 'a').
    This means that in second SP blob will be created as STREAM (unlike it is in 1st proc).

    Then we make copy of this blob <N_COUNT_PER_MEASURE> times to another blob (in each SP).
    Duration of this SP execution is measured as difference between psutil.Process(fb_pid).cpu_times() counters.

    We do these measures <N_MEASURES> times for each SP, and each result is added to the list.
    Finally, we evaluate RATIO between duration of SP-1 and SP-2, and then obtain MEDIAN for that.
    This median must not exceed <MAX_RATIO> threshold.
    NB! It is desirable that test database will use page cache with big enough size
    (in order to avoid impact of blob creation on IO performance).
    Because of this, test DB will use info from predefined <REQUIRED_ALIAS> in the $QA_ROOT/files/qa-database.conf

    Before this improvement median ratio was 5.0 ... 6.0 (5.0.0.821). After this improvement (5.0.0.824) it is 1.25 ... 1.33.
    Values of median are equal on Windows and Linux, thus MAX_RATIO = 2 for both OS.

NOTES:
    [16.02.2023] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has alias defined by variable REQUIRED_ALIAS.
       Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)

    Checked on:
        LINUX Debian 10, FB 5.0.0.930 SS/CS
        Windows 8.1, FB 5.0.0.824 SS/CS

    [19.07.2023] pzotov
    Increased values of N_BLOB_FINAL_LEN and N_COUNT_PER_MEASURE, reduced value of N_MEASURES: every iteration must perform
    significant volume of job in order its "cpu_time().user" counter will be not too small/inaccurate.
    Changes caused by permanent fails when FB is running as SuperServer. Discussed with Vlad.

    Checked again on 5.0.0.1121, mode = SuperServer
    1) Windows-10 v 21H2 bld 19044.3208 (IBSurgeon test server):
        sp_blob_copy_1 sp_blob_copy_2 ratio
        0.234375       0.171875       1.363636
        0.218750       0.171875       1.272727
        0.250000       0.140625       1.777778
        0.265625       0.203125       1.307692
        0.234375       0.125000       1.875000
        0.250000       0.171875       1.454545
        0.250000       0.156250       1.600000
        0.250000       0.140625       1.777778
        0.265625       0.218750       1.214286
        0.218750       0.171875       1.272727
        0.203125       0.234375       0.866667
        Ratio median:     1.363636

    2) Windows 8.1 v 6.3 bld 9600 (home test machine):
        sp_blob_copy_1 sp_blob_copy_2 ratio
        0.484375       0.359375       1.347826
        0.375000       0.390625       0.960000
        0.453125       0.406250       1.115385
        0.406250       0.343750       1.181818
        0.390625       0.359375       1.086957
        0.421875       0.359375       1.173913
        0.437500       0.406250       1.076923
        0.421875       0.343750       1.227273
        0.453125       0.296875       1.526316
        0.468750       0.406250       1.153846
        0.437500       0.437500       1.000000
        Ratio median:     1.153846
"""

import os
import time
import psutil
import pytest
from firebird.qa import *

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

# Pre-defined alias for test DB in the QA_root/files/qa-databases.conf.
# This file (qa-databases.conf) must be copied manually to each testing
# FB home folder, with replacing databases.conf there:
#
REQUIRED_ALIAS = 'tmp_gh_7382_alias'

###########################
###   S E T T I N G S   ###
###########################

# How many times we call procedures:
N_MEASURES = 11

N_BLOB_FINAL_LEN = 32000

# How many iterations must be done:
N_COUNT_PER_MEASURE = 2000

# Maximal value for MEDIAN of ratios between CPU user time when comparison was made.
# On Windows 8.1 and Linux Debian this median ratio is ~1.25 ... 1.33 
#
MAX_RATIO = 2 if os.name == 'nt' else 2
#######################################

init_script = \
f'''
set term ^;
create or alter procedure sp_blob_copy_1 (
    n_len int
    ,n_cnt int
) as
    declare b_src blob sub_type text character set unicode_fss;
    declare b_tgt blob sub_type text character set utf8;
    declare i int = 0;
begin
    b_src = lpad('',n_len, 'a');

    i = 0;
    while (i < n_cnt) do
    begin
        -- copy_blob
        b_tgt = b_src;
        i = i + 1;
    end
end
^
create or alter procedure sp_blob_copy_2 (
    n_len int
    ,n_cnt int
) as
    declare b_src blob sub_type text character set unicode_fss;
    declare b_tgt blob sub_type text character set utf8;
    declare i int = 0;
begin
    while (i < n_len) do
    begin
        b_src = blob_append(b_src, 'a');
        i = i + 1;
    end

    i = 0;
    while (i < n_cnt) do
    begin
        -- copy_blob
        b_tgt = b_src;
        i = i + 1;
    end
end
^
commit
^
'''

db = db_factory(filename = '#' + REQUIRED_ALIAS, init = init_script, charset = 'win1251' )
act = python_act('db')

expected_stdout = """
    Duration ratio, median: acceptable
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
            for sp_suffix in ('1','2'):
                sp_name = f'sp_blob_copy_{sp_suffix}'
                fb_info_init = psutil.Process(fb_pid).cpu_times()
                cur.callproc( sp_name, (N_BLOB_FINAL_LEN, N_COUNT_PER_MEASURE,) )
                #con.commit()
                fb_info_curr = psutil.Process(fb_pid).cpu_times()
                #sp_time[ sp_name, i ]  = max(fb_info_curr.user+fb_info_curr.system - fb_info_init.user - fb_info_init.system, 0.000001)
                sp_time[ sp_name, i ]  = max(fb_info_curr.user - fb_info_init.user, 0.000001)

    ratio_lst = []
    for i in range(0, N_MEASURES):
        ratio_lst.append( sp_time['sp_blob_copy_2',i]  / sp_time['sp_blob_copy_1',i]  )
    median_ratio = median(ratio_lst)

    print( 'Duration ratio, median: ' + ('acceptable' if median_ratio <= MAX_RATIO else '/* perf_issue_tag */ POOR: %s, more than threshold: %s' % ( '{:9g}'.format(median_ratio), '{:9g}'.format(MAX_RATIO) ) ) )

    if median_ratio > MAX_RATIO:
        print('Ratio statistics for %d measurements:' % N_MEASURES)
        print('sp_blob_copy_1 sp_blob_copy_2 ratio')
        for i in range(0, N_MEASURES):
            print( '%14.6f %14.6f %14.6f' % (sp_time['sp_blob_copy_2',i], sp_time['sp_blob_copy_1',i], sp_time['sp_blob_copy_2',i]  / sp_time['sp_blob_copy_1',i]) )
        print('Ratio median: %12.6f' % median_ratio)

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
