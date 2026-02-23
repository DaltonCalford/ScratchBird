#coding:utf-8

"""
ID:          issue-5175
ISSUE:       5175
TITLE:       Better performance of creating packages containing many functions
DESCRIPTION:
    ###### ACHTUNG ######
    Package 'psutil' must be installed before run this test!
    #####################

    Test creates package with N_LIMIT functions and the same number of standalone PSQL functions.
    We measure separately N_MEASURES times duration of:
        1. PSQL object(s) compilation and
        2. COMMIT of their BLR
    Fix that was done for this ticket relates mostly to the RATIO between "1" and "2" and (secondary) to their
    absolute values:
    -------------------------------------------------------------------------------------------------------------------
    |                        |   Package with 5'000 functions     |            |   5'000 standalone functions         |
    |                        |------------------------------------|            |--------------------------------------|
    |                        |compile    commit     time ratio:   |            |compile    commit      time ratio:    |
    |Build                   |time, ms   time, ms   commit/compile|            |time, ms   time, ms    commit/compile |
    |------------------------|------------------------------------|------------|--------------------------------------|
    |3.0.0.31374 Beta 1      |    8662     108479       12.52     |            |    6066     114489         18.87     |
    |3.0.0.31896 Beta 2      |    8674     113157       13.05     |            |    5868     113167         19.29     |
    |3.0.0.32136 RC 1        |    9601       5348        0.56     |            |    5934       6724          1.06     |
    |3.0.8.33540             |   11308       5794        0.51     |            |    4445       5617          1.26     |
    |4.0.1.2672              |    9841       6130        0.62     |            |    4864       6413          1.32     |
    |5.0.0.321               |   10734       4346        0.40     |            |    4803       6497          1.35     |
    -------------------------------------------------------------------------------------------------------------------
                                                         ^^^^                                                ^^^^
    One may see that although compiling time increased for package and reduced for standalone functions (since RC-1),
    time ot COMMIT was reduced  significantly.

    Test was fully re-implemented in order to take in account exactly *this* change rather than wrong way (when we
    did analysis of ratio between *total* time of compilation + commit for package vs N_LIMIT standalone functions).

    Within each measure we create new database, make its FW equals to OFF and do following:
    * create poackage with N_LIMIT functions
    * commit
    * create N_LIMIT standalone functions
    * commit

    Values of CPU User Time before and after every action are taken. Difference between them can be consider as much more
    accurate time estimation. Then we calculate RATIO between these differences, and so we can evaluate how long COMMIT was
    in comparison to DML.
    We perform this N_MEASURES times and get list of ratios for both cases (i.e. one list for package with N_LIMIT functions
    and second for N_LIMIT standalone functions). Then we evaluate MEDIAN of ratios for each list.
    Finally, we compare these medians with THRESHOLDS, which values have been taken from the table that is shown above,
    with increasing by approx. 20%
JIRA:        CORE-4880
FBTEST:      bugs.core_4880
NOTES:
    [28.05.2022] pzotov
    Checked on 5.0.0.501, 4.0.1.2692, 3.0.8.33535

    [14.09.2022] pzotov
    1. It is better to made func 'median()' separate rather than inner.
       See letter from pcisar, 31-may-2022 11:25:
         "In Python, inner function is created anew whenever you call the function,
         just for the time of function invocation. It has it's uses and advantages,
         but it's not generally recommended (it's not like in other languages!)"
    2. Additional testing after get fail with weird message:
       "psutil.NoSuchProcess: process PID not found (pid=16652)" for 3.0.8 Classic,
       captured for: fb_info_b = psutil.Process(fb_pid).cpu_times().
       This could be caused by FB crash. TO BE INVESTIGATED FURTHER.
"""

import os
import psutil
import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

###########################
###   S E T T I N G S   ###
###########################
# How many times we call PSQL code (two stored procedures:
# one for performing comparisons based on LIKE, second based on SIMILAR TO statements):
N_MEASURES = 21

# How many functions must be created on each iteration:
N_LIMIT = 1500

if os.name == 'nt':
    MAX_TIME_RATIOS_COMMIT_TO_COMPILE = {'packaged' : 0.55, 'standalone' : 0.75}
else:
    MAX_TIME_RATIOS_COMMIT_TO_COMPILE = {'packaged' : 0.55, 'standalone' : 0.50}


#--------------------------------------------------------------------
def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None
#--------------------------------------------------------------------

@pytest.mark.perf_measure
@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    act.db.set_async_write() # just to be sure, although this is by default

    expected_stdout = f"""
        Median value for {N_MEASURES} ratios of commit_time/compile_time when create {N_LIMIT} packaged PSQL objects: acceptable
        Median value for {N_MEASURES} ratios of commit_time/compile_time when create {N_LIMIT} standalone PSQL objects: acceptable
    """

    func_headers_ddl = ''.join( [ '\n  function fn_%d returns int;' % i for i in range(N_LIMIT) ] )
    func_bodies_ddl = ''.join( [ '\n  function fn_%d returns int as begin return %d; end' % (i,i) for i in range(N_LIMIT) ] )

    pkg_header_ddl = '\n'.join( ('create or alter package huge as\nbegin', func_headers_ddl, 'end') )
    pkg_body_ddl = '\n'.join( ('recreate package body huge as\nbegin', func_bodies_ddl, 'end') )

    sp_time = {}
    with act.db.connect() as con:

        with con.cursor() as cur:
            cur.execute('select mon$server_pid as p from mon$attachments where mon$attachment_id = current_connection')
            fb_pid = int(cur.fetchone()[0])

        for i in range(0, N_MEASURES):
            for ftype in ('packaged', 'standalone'):

                fb_info_a = psutil.Process(fb_pid).cpu_times()
                if ftype == 'packaged':
                    con.execute_immediate(pkg_header_ddl)
                    con.execute_immediate(pkg_body_ddl)
                else:
                    for k in range(0,N_LIMIT):
                        con.execute_immediate( 'create or alter function sf_%d returns int as begin return %d; end' % (k,k)  )

                fb_info_b = psutil.Process(fb_pid).cpu_times()
                con.commit()
                fb_info_c = psutil.Process(fb_pid).cpu_times()

                cpu_time_for_compile = max(fb_info_b.user - fb_info_a.user, 0.000001)
                cpu_time_for_commit = fb_info_c.user - fb_info_b.user

                sp_time[ ftype, i ]  = cpu_time_for_commit / cpu_time_for_compile

    commit_2_compile_ratio_for_package = [round(v,2) for k,v in sp_time.items() if k[0] == 'packaged']
    commit_2_compile_ratio_for_standal = [round(v,2) for k,v in sp_time.items() if k[0] == 'standalone']

    actual_time_ratios_commit2compile = { 'packaged' : median(commit_2_compile_ratio_for_package), 'standalone': median(commit_2_compile_ratio_for_standal) }

    for ftype in ('packaged', 'standalone'):
        msg = f"Median value for {N_MEASURES} ratios of commit_time/compile_time when create {N_LIMIT} {ftype} PSQL objects: "
        if actual_time_ratios_commit2compile[ ftype ] < MAX_TIME_RATIOS_COMMIT_TO_COMPILE[ ftype ]:
            print( msg + 'acceptable')
        else:
            print( msg + '/* perf_issue_tag */ UNACCEPTABLE: %12.2f -- greater than threshold = %12.2f' % (actual_time_ratios_commit2compile[ ftype ],  MAX_TIME_RATIOS_COMMIT_TO_COMPILE[ ftype ]) )
            print( 'Check result of %d measures:' % N_MEASURES )

            # List with concrete values (source for median evaluation):
            lst = commit_2_compile_ratio_for_package if ftype == 'packaged' else commit_2_compile_ratio_for_standal
            for i,p in enumerate(lst):
                print('%3d' % i, ':', '%12.2f' % p)


    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
