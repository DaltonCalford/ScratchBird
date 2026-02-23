#coding:utf-8

"""
ID:          issue-6481
ISSUE:       6481
TITLE:       Performance problem when using SRP plugin
DESCRIPTION:
  :::::::::::::::::::: N O T A   B E N E  :::::::::::::::::
  It is crucial for this test that firebird.conf have following _SEQUENCE_ of auth-plugins:  Srp, ...,  Legacy_Auth
  -- i.e. Srp must be specified BEFORE Legacy.
  Slow time of attach establishing can NOT be seen otherwise; rather almost no difference will be in that case.
  :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

  Test creates two users: one using Legacy plugin and second using Srp.
  Then we make ~20...30 pairs of attach/detach by each of these users and get total time difference for these actions.
  Ratio between these total differences must be limited with threshold. Its value was determined after dozen of runs
  and it seems to be reasonable assign to it value 1.5 (see MIN_RATIO_THRESHOLD in the code).

  Test output will contain ALERT if total time of <attaches_using_Srp> vs <attaches_using_Legacy>
  will be greater than MIN_RATIO_THRESHOLD.
  NB: median ratios must be ignored if both medians have proximity to zero, usually this is ~20...30 ms.
  This is tuned by setting MAX_MEDIAN_TO_IGNORE.

  Reproduced on several builds 4.x before 17.01.2020 (tested: 4.0.0.1712 CS, 4.0.0.1731 CS - got ratio = ~1.95).
  Reproduced also on 3.0.5.33221 Classic - got ratio ~1.50 ... 1.70; could NOT reproduce on 3.0.5 SuperClassic / SuperServer.
JIRA:        CORE-6237
FBTEST:      bugs.core_6237
NOTES:
    [09.02.2022] pcisar
        Fails on Windows 4.0.1 with ratio 1.98 - raw iron W10, does not fail with Linux on the same HW
    [16.09.2022] pzotov
        Checked on Windows and Linux, 4.0.1.2692, 3.0.8.33535.
    [07.10.2022] pzotov
    2DO: WireCrypt must be disabled for this test. Custom driver-config object must be used for DPB.
"""

import pytest
import datetime
import platform
from firebird.qa import *

db = db_factory()

leg_user = user_factory('db', name='tmp_c6237_leg', password='123', plugin='Legacy_UserManager')
srp_user = user_factory('db', name='tmp_c6237_srp', password='123', plugin='Srp')

act = python_act('db')

###########################
###   S E T T I N G S   ###
###########################
# Number of measurements:
N_COUNT = 51

# Minimal ratio between medians of time for alerting:
MIN_RATIO_THRESHOLD = 1.50

# Maximal value for both medians of time (Srp and Legacy) when
# ratio between them can be ignored because of proximity to zero:
MAX_MEDIAN_TO_IGNORE = 50

#------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#------------------

@pytest.mark.version('>=3.0.5')
def test_1(act: Action, leg_user: User, srp_user: User, capsys):
    
    sp_time = {}

    for user in [leg_user, srp_user]:
        for i in range(N_COUNT):
             start = datetime.datetime.now()
             stop = start
             with act.db.connect(user=user.name, password=user.password):
                stop = datetime.datetime.now()

             diff = stop - start
             sp_time[user.name, i] = int(diff.seconds) * 1000 + diff.microseconds / 1000

    leg_user_conn_time = [round(v,2) for k,v in sp_time.items() if k[0] == leg_user.name]
    srp_user_conn_time = [round(v,2) for k,v in sp_time.items() if k[0] == srp_user.name]

    conn_time_ratio = median(srp_user_conn_time) / median(leg_user_conn_time)

    expected_txt = 'EXPECTED. Ratio median(Srp) / madian(Legacy) is less then threshold or medians are about zero.'
    if conn_time_ratio < MIN_RATIO_THRESHOLD or max( median(srp_user_conn_time), median(leg_user_conn_time) ) < MAX_MEDIAN_TO_IGNORE:
        print(expected_txt)
    else:
        print(f'/* perf_issue_tag */ Ratio Srp/Legacy: {conn_time_ratio} - is GREATER than threshold = {MIN_RATIO_THRESHOLD}.')
        print(f'    Median for Srp: {median(srp_user_conn_time)}')
        print(f'    Median for Legacy: {median(leg_user_conn_time)}')

        print('Check srp_user_conn_time:')
        print(srp_user_conn_time)

        print('\\nCheck leg_user_conn_time:')
        print(leg_user_conn_time)
        

    act.expected_stdout = expected_txt
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
