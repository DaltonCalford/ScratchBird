#coding:utf-8

"""
ID:          issue-6976
ISSUE:       6976
TITLE:       Lack of proper clean up after failure to initialize shared memory
DESCRIPTION:
  Test reads content of empty database (which is created auto) and makes several checks
  by writing this content to another .fdb file but WITHOUT last <N> pages.
  We have to start cut off last N pages and verisy that every time engine will instantly
  detect damage of database (i.e. not waiting for ~110 seconds as it was before fix).
  It is useless to cut off *last* 2..3 pages because engine makes reserving of space.

  Test settings for 'starting' page to be cuted off and number of pages are:
    SKIP_BACK_FROM_LAST_PAGE;
    NUM_OF_CUTED_LAST_PAGES

  Confirmed on WI-V4.0.1.2606: needed to wait for exactly 110s after each page starting from N-3.
  Checked on 3.0.8.33501, 4.0.1.2613.
FBTEST:      bugs.gh_6976

NOTES:
    [20.07.2022] pzotov
    Bug reproduced on 4.0.1.2606 but only when it is tested just after 5.0.0.591.
    In that case firebird.log will have following messages:

        HOME-AUX2	Thu Jul 21 01:25:17 2022
        I/O error during "ReadFile" operation for file "C:/TEMP/.../TMP_GH_6976.CUTED_OFF.FDB"
        Error while trying to read from file
        [localized message can be here]: "end of file encountered"


        HOME-AUX2	Thu Jul 21 01:25:17 2022 ------------------------------------ [ 1 ]
        TPC: Cannot initialize the shared memory region (header)
        I/O error during "ReadFile" operation for file "C:/TEMP/.../TMP_GH_6976.CUTED_OFF.FDB"
        Error while trying to read from file
        [localized message can be here]: "end of file encountered"


        HOME-AUX2	Thu Jul 21 01:27:07 2022 ------------------------------------ [ 2 ] diff: 110 sec.
        Operating system call WaitForSingleObject failed. Error code 0


        HOME-AUX2	Thu Jul 21 01:27:07 2022
        TPC: Cannot initialize the shared memory region (header)
        operating system directive WaitForSingleObject failed
        [localized message can be here]: "operation completed successfully"

    NB: time interval between 01:25:17 and 01:27:07 is exactly 110 seconds.
    On recent FB 4.x and 5.x median duration for delivering error to client must be about 80 ms.
    On 4.0.1.2606 this was about 640 ms (in case when it was tested without previous 5.x test).

    For concluding whether problem exists or no, MEDIAN value of time serie is used.
    Recent FB build show that time to wait exception with expected gdscode = 335544344 must be
    less than 300 ms but it depends on ServerMode (for Classic it is about 2x more than for Super).
    Median value will be compared with THRESHOLD_FOR_MAKE_CONNECT_MS variable..
    Each attempt to connect top broken DB must bring stack with TWO gdscodes:
        isc_io_error = 335544344;
        isc_io_read_err = 335544736;
    Checked on Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.730

    [26.02.2025] pzotov
    Increased value THRESHOLD_FOR_MAKE_CONNECT_MS for servermode = 'Super': on poor hardware
    previous value was not enough. Problem appeared on 6.0.0.655, Windows 10, build 19045.3086.

    [10.10.2025] pzotov
    Increased SKIP_BACK_FROM_LAST_PAGE from 15 to 50: test stopped passing on FB 6.x after
    3e3b75 ("Re-add indexes for object name without schema name...") with issuing
    "UNEXPECTED: can connect to DB with missed last ... pages".
    Checked on 6.0.0.1300 5.0.4.1711 4.0.7.3236 3.0.14.33826.

    [28.12.2025] pzotov
    Increased threshold value for median time that engine spends to establish connection for Classic.
    Detected on 6.0.0.1383 after migration of QA suite on reserving host (Windows 10, CPU W-2123 3.60GHz).
    Previous value was 250, new median is ~350. The reason of such change is unknown.
    *** TODO ***
    Probably, test must be re-implemented: we have to check only absense of delay for ~110 seconds between
    second and third attempts to make connection.
"""

import os
import subprocess
import codecs
import re
import time
from datetime import datetime as dt
from datetime import timedelta
from pathlib import Path

import pytest
from firebird.qa import *

##############################
###     s e t t i n g s    ###
##############################
# Threshold values for median time that engine spent to establish connection to broken DB, ms.
# We have to split these values depending on ServerMode.
MAX_WAIT_FOR_CONNECT_CS = 500
MAX_WAIT_FOR_CONNECT_SS = 150
SKIP_BACK_FROM_LAST_PAGE = 50
NUM_OF_CUTED_LAST_PAGES = 20
#############################

db = db_factory()
tmp_fdb = db_factory(filename = 'tmp_gh_6976.cuted_off.fdb', async_write = True)

act_source = python_act('db')
act_broken = python_act('tmp_fdb')

#--------------------------------------------

def median(lst):
    n = len(lst)
    s = sorted(lst)
    return (sum(s[n//2-1:n//2+1])/2.0, s[n//2])[n % 2] if n else None

#--------------------------------------------

def try_cuted_off_db(act_source, act_broken, db_page_size, db_pages_cnt, cut_off_pages_cnt):

    dbsrc = act_source.db.db_path
    dbtgt = act_broken.db.db_path
    with open(dbsrc, 'rb') as f:
        fdb_binary_content = f.read( db_page_size * (db_pages_cnt - cut_off_pages_cnt) )

    with open(dbtgt, 'wb') as w:
        w.write(fdb_binary_content)

    #msg_suffix = 'on attempt to connect to DB with missed last %d pages' % cut_off_pages_cnt

    da = dt.now()
    diff_ms = -1

    # isc_io_error = 335544344;
    # isc_io_read_err = 335544736;
    expected_gds_list = (335544344, 335544736)
    try:
        #########################
        ###   c o n n e c t   ###
        #########################
        with act_broken.db.connect():
           #with act_broken.db.connect(user = 'John', password = '123'):
           # NOTE: this *can occur.
           # In that case one need to increase SKIP_BACK_FROM_LAST_PAGE setting.
           print('UNEXPECTED: can connect to DB with missed last %d pages' % cut_off_pages_cnt)

    except Exception as e:
        db = dt.now()
        diff_ms = (db-da).seconds*1000 + (db-da).microseconds//1000
        if set( expected_gds_list ).issubset( set(e.gds_codes) ):
            pass
        else:
            print('Actual list of gdscodes:')
            print(e.gds_codes)
            print('- does not contain at least one element from following list:')
            print(expected_gds_list)
    finally:
        # We have to restore VALID content of target database
        # otherwise problem on test teardown will raise:
        with open(dbsrc, 'rb') as f:
            fdb_origin_content = f.read( db_page_size * db_pages_cnt )
        with open(dbtgt, 'wb') as w:
            w.write(fdb_origin_content)


    return diff_ms

#--------------------------------------------

@pytest.mark.version('>=3.0.8')
@pytest.mark.platform('Windows')
def test_1(act_source: Action, act_broken: Action, capsys):

    THRESHOLD_FOR_MAKE_CONNECT_MS = MAX_WAIT_FOR_CONNECT_CS if 'classic' in act_source.vars['server-arch'].lower() else MAX_WAIT_FOR_CONNECT_SS

    with act_source.db.connect() as con:
        db_page_size = con.info.page_size
        db_pages_all = con.info.pages_allocated
        db_pages_cnt = con.info.size_in_pages

    ##################
    ###   L O O P  ###
    ##################
    connect_establishing_ms = []
    for i in range(SKIP_BACK_FROM_LAST_PAGE, SKIP_BACK_FROM_LAST_PAGE + NUM_OF_CUTED_LAST_PAGES, 1):
        ms = try_cuted_off_db( act_source, act_broken, db_page_size, db_pages_cnt, i)
        if ms >= 0:
            connect_establishing_ms.append( ms )

    msg_prefix = 'Median duration of receiving an error by the client: '
    if len(connect_establishing_ms) > 0:
        median_connect_ms = median(connect_establishing_ms)
        if median_connect_ms <= THRESHOLD_FOR_MAKE_CONNECT_MS:
            print(msg_prefix + 'acceptable.')
        else:
            print(msg_prefix + '/* perf_issue_tag */ POOR: %s - more than threshold: %s' % ( '{:9g}'.format(median_connect_ms), '{:9g}'.format(THRESHOLD_FOR_MAKE_CONNECT_MS)  ))
            print("Check values:" )
            for p in connect_establishing_ms:
                print(p)

    else:
        print('UNEXPECTED FINAL: ENGINE COULD NOT DETECT DAMAGE OF DATABASE.')


    expected_stdout = msg_prefix + 'acceptable.'
    act_source.expected_stdout = expected_stdout
    act_source.stdout = capsys.readouterr().out
    assert act_source.clean_stdout == act_source.clean_expected_stdout

