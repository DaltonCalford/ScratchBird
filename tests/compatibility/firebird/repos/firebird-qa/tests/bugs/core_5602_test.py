#coding:utf-8

"""
ID:          issue-5868
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5868
TITLE:       Slow changes on domain
DESCRIPTION:
JIRA:        CORE-5602
FBTEST:      bugs.core_5602

NOTES:
    [08.03.2023] pzotov
    Re-implemented: we have to use psutil package instead of 'fragile' datediff.
    Confirmed bug on 3.0.2.32703 (date of build: 21-mar-2017).
    Timedelta for 'alter domain ... add|drop constraint' could achieve ~110 seconds:
        2023-03-08T14:27:09.6540 (16460:00000000032C0040) EXECUTE_STATEMENT_START
        alter domain bool_emul drop constraint
        2023-03-08T14:29:01.0190 (16460:00000000032C0040) EXECUTE_STATEMENT_FINISH
        alter domain bool_emul drop constraint

    Checked on 3.0.11.33665, 4.0.3.2904, 5.0.0.970
"""
import os
import psutil
import pytest
from firebird.qa import *
import time

#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

###########################
###   S E T T I N G S   ###
###########################
# How many measures we will perform:
N_MEASURES = 11

# Maximal value for MEDIAN of ratios between CPU user time when comparison was made,
# time_for_alter_domain_(add_|_drop)_constraint and time_for_transaction_commit.
# On march-2023, medians are almost equal and are about 0.35 ... 0.45 (in FB 3.x, 4.x and 5.x)
#
############################
DEL_2_COMMIT_MAX_RATIO = 0.8
ADD_2_COMMIT_MAX_RATIO = 0.8
############################

db = db_factory(from_backup='core5602.fbk')
act = python_act('db')

@pytest.mark.perf_measure
@pytest.mark.version('>=3.0.3')
def test_1(act: Action, capsys):
    time_data = {}
    with act.db.connect() as con:
        cur=con.cursor()
        cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
        fb_pid = int(cur.fetchone()[0])

        for i in range(0, N_MEASURES):
            fb_info_1 = psutil.Process(fb_pid).cpu_times()
            con.execute_immediate(f'alter domain bool_emul drop constraint -- {i}')
            fb_info_2 = psutil.Process(fb_pid).cpu_times()
            con.commit()
            fb_info_3 = psutil.Process(fb_pid).cpu_times()
            time_data[ 'del_constraint', i ] = ( max(fb_info_2.user - fb_info_1.user, 0.000001), max(fb_info_3.user - fb_info_2.user, 0.000001)  )

            con.execute_immediate(f"alter domain bool_emul add check (value in ('t', 'f')) -- {i}")
            fb_info_4 = psutil.Process(fb_pid).cpu_times()
            con.commit()
            fb_info_5 = psutil.Process(fb_pid).cpu_times()
            time_data[ 'add_constraint', i ] = ( max(fb_info_4.user - fb_info_3.user, 0.000001), max(fb_info_5.user - fb_info_4.user, 0.000001)  )


    del_constraint_to_commit_ratios = [ v[0] / v[1] for k,v in time_data.items() if k[0] == 'del_constraint' ]
    add_constraint_to_commit_ratios = [ v[0] / v[1] for k,v in time_data.items() if k[0] == 'add_constraint'  ]

    #for k,v in sorted(time_data.items()):
    #    print(k,':::',v)
    #print(del_constraint_to_commit_ratios)
    #print(add_constraint_to_commit_ratios)

    del_constr_to_commit_median = median(del_constraint_to_commit_ratios)
    add_constr_to_commit_median = median(add_constraint_to_commit_ratios)

    msg_del_success = 'ALTER DOMAIN DROP CONSTRAINT performed for acceptable time'
    msg_add_success = 'ALTER DOMAIN ADD CONSTRAINT performed for acceptable time'

    if del_constr_to_commit_median < DEL_2_COMMIT_MAX_RATIO:
        print(msg_del_success)
    else:
        print('/* perf_issue_tag */ ALTER DOMAIN DROP CONSTRAINT perfomed too slow. Ratios of DML to COMMIT time:')
        for p in del_constraint_to_commit_ratios:
            print('%12.4f' % p)
        print('Median value: %12.4f - GREATER than threshold: %12.4f' % (del_constr_to_commit_median,DEL_2_COMMIT_MAX_RATIO))
       

    if add_constr_to_commit_median < ADD_2_COMMIT_MAX_RATIO:
        print(msg_add_success)
    else:
        print('ALTER DOMAIN ADD CONSTRAINT perfomed too slow. Ratios of DML to COMMIT time:')
        for p in add_constraint_to_commit_ratios:
            print('%12.4f' % p)
        print('Median value: %12.4f - GREATER than threshold: %12.4f' % (add_constr_to_commit_median,ADD_2_COMMIT_MAX_RATIO))

    expected_stdout = '''
        ALTER DOMAIN DROP CONSTRAINT performed for acceptable time
        ALTER DOMAIN ADD CONSTRAINT performed for acceptable time
    '''

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
