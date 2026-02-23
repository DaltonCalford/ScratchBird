#coding:utf-8

"""
ID:          issue-7031
ISSUE:       7031
TITLE:       gbak -b failed with "invalid transaction handle (expecting explicit transaction start)"
DESCRIPTION:
  Confirmed bug on 3.0.8.33524, 4.0.0.2642, 5.0.0.271:
  backup not completed (.fbk file was not created), got following:
    * in the STDERR of gbak (only in FB 3.0.8; NOT in FB 4.0 and 5.0):
        gbak: ERROR:invalid transaction handle (expecting explicit transaction start)
        gbak:Exiting before completion due to errors
    * in firebird.log (for all: 3.0.8, 4.0, 5.0):
        internal Firebird consistency check (page in use during flush (210), file: cch.cpp line: NNN)
  Checked on 3.0.8.33525
FBTEST:      bugs.gh_7031

NOTES:
    [21.07.2022] pzotov
    Confirmed problem:
    1) on WI-V3.0.7.33374 gbak fails with
       "ERROR:invalid transaction handle (expecting explicit transaction start) / Exiting before completion due to errors";
       BUGCHECK message will be added to the firebird.log:
       internal Firebird consistency check (page in use during flush (210), file: cch.cpp line: 2750)
    2) on WI-V3.0.8.33520 gbak hangs but BUGCHECK message appears in the firebird.log:
       internal Firebird consistency check (page in use during flush (210), file: cch.cpp line: 2761)
    3) firebird.log may contain (in common case) non-ascii messages, they even can be encoded in different codepages.
       Moreover, this log can rarely be broken and some binary fragments can be met there.
       Attempt to read such log can fail with UnicodeDecode. To prevent this, one need either to specify encoding_errors = ignore
       on every call of act.connect_server(), or (MUCH MORE PRERERABLE) we must create section with name = [DEFAULT] in the
       firebird-driver.conf and add there parameter "encoding_errors = ignore".
       This test assumes that firebird-driver.conf was adjusted in this way, so calls for obtaining content of firebird.log
       (i.e. "with act.connect_server() as srv") are used WITHOUT any additional parameters.

    Checked on 3.0.8.33501, 4.0.1.2613, 5.0.0.591
"""

import pytest
from firebird.qa import *

from pathlib import Path
from firebird.driver import TPB
from difflib import unified_diff
import subprocess
import time

MAX_TIME_FOR_BACKUP = 30

db = db_factory(page_size = 4096)

act = python_act('db', substitutions=[('[ \t]+', ' ')])

test_fbk = temp_file('tmp_gh_7031-tmp.fbk')
bkup_log = temp_file('tmp_gh_7031-bkp.log')
bkup_msg = 'Backup COMPLETED.'

@pytest.mark.version('>=3.0.7')
def test_1(act: Action, test_fbk: Path, bkup_log: Path, capsys):
    
    # 1. Start 'initial' Tx and perform start + commit lot of subsequent transactions:
    with act.db.connect() as con:
        tx_list = []
        tx0 = con.transaction_manager()

        # start 1st Tx:
        tx_list.append( tx0.begin() )
        tx_init = con.info.get_active_transaction_ids()[0]  # get just started Tx ID
        tx_per_TIP = (con.info.page_size - 20) * 4
        tx_last = (tx_init//tx_per_TIP + 1) * tx_per_TIP - 1;
      
        # Start and commit all subsequent transactions, in order to make DB with
        # OIT = 1 and Next_Tx = 16303 (but _not_ 16304!!)
        # NOTE. It is crucial to make loop on the scope tx_init ... (tx_last-1),
        # but NOT up to tx_last! Otherwise problem will not be reproduced.
        #
        for i in range(tx_init, tx_last-1):
            txi = con.transaction_manager()
            txi.begin()
            txi.commit()

        tx0.rollback()

    with act.connect_server() as srv:
        srv.info.get_log()
        fb_log_init = srv.readlines()
    

    bkp_retcode = -1

    # 2. Run gbak with MAX ALLOWED TIMEOUT.
    # If the timeout expires, the child process will be killed and waited for.
    # The TimeoutExpired exception will be re-raised after the child process has terminated.
    try:
        p = subprocess.run([ act.vars['gbak'],
                            '-user', act.db.user, '-password', act.db.password,
                            '-b', act.db.db_path, str(test_fbk),
                            '-v', '-y', str(bkup_log)
                          ]
                          ,stderr = subprocess.STDOUT
                          ,timeout = MAX_TIME_FOR_BACKUP
                         )
        if p.returncode == 0:
            print( bkup_msg )
        else:
            with open(bkup_log, 'r') as f:
                print(f.read())
    except Exception as e:
        print(e.__str__())

    with act.connect_server() as srv:
        srv.info.get_log()
        fb_log_curr = srv.readlines()
 

    # 3. Compare old and new content of firebird.log:
    for line in unified_diff(fb_log_init, fb_log_curr):
        if line.startswith('+'):
            print(line.strip())

    
    act.expected_stdout = bkup_msg
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
