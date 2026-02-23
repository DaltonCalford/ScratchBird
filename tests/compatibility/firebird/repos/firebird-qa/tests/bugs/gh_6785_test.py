#coding:utf-8

"""
ID:          issue-6785
ISSUE:       6785
TITLE:       Problem when restoring the database on FB 4.0 RC1 (gbak regression)
DESCRIPTION:
    Test used database backup that was provided in the ticket.

    Maximal allowed time is set here for restoring process and gbak will be
    forcedly killed if it can not complete during this time.
    Currently this time is 300 seconds (see 'MAX_THRESHOLD' variable).

    Database is validated (using 'gfix -v -full') after successful restore finish.
    Test checks that returned codes for both gbak and validation are zero.
    Also, it checks that firebird.log contains message with 'all-zeroes' values
    for validation outcome ("Validation finished: 0 errors, 0 warnings, 0 fixed").

    Restore issues warnings:
      gbak: WARNING:function F_DATETOSTR is not defined
      gbak: WARNING:    module name or entrypoint could not be found
      gbak: WARNING:function F_DATETOSTR is not defined
      gbak: WARNING:    module name or entrypoint could not be found
    All of them are ignored by this test when gbak output is parsed.

    Confirmed bug on 4.0.0.2452 SS: gbak infinitely hanged.
    Checked on 4.0.0.2453 SS/CS (Linux and Windows): all OK, restore lasts near 200s.
FBTEST:      bugs.gh_6785
NOTES:
    [30.06.2022] pzotov
    Checked again on 4.0.1.2692, 5.0.0.509. Confirmed reproducing of problem on 4.0.0.2452.

    [20.07.2022] pzotov
    firebird.log may contain messages encoded in different code pages (say, in cp1251 and in utf8).
    Because of this, one need to IGNORE any decoding errors when obtain content of log.
    In this case call of act.get_firebird_log() will raise:
        UnicodeDecodeError: 'ascii' codec can't decode byte 0x** ... ordinal not in range(128)

    We have either to specify this using somewhat like "act.connect_server(encoding_errors = 'ignore')",
    or to change firebid-driver.conf - and this will be more proper way.
    This file (firebid-driver.conf) must have section with name [DEFAULT] with encoding_errors = ignore.
    Test assumes exactly THIS, i.e. we do NOT specify "encoding_errors = 'ignore'" in acty.connect_server()

    [30.09.2025] pzotov
    Separated MAX_THRESHOLD assignment depending on OS (on Linux currently it must be greater than on Windows).
    Added diagnostics when restore command failed (usually because of timeout).
"""
import os
import locale
import zipfile
import subprocess
import re
from difflib import unified_diff
from textwrap import wrap
from firebird.driver import SrvRepairFlag
from pathlib import Path
import time

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db')

tmp_log = temp_file('gh_6785.tmp.log')
tmp_fbk = temp_file('gh_6785.tmp.fbk')
tmp_fdb = temp_file('gh_6785.tmp.fdb')

###############################################
MAX_THRESHOLD = 300 if os.name == 'nt' else 400
###############################################

expected_stdout = """
    Restore retcode: 0
    Validation retcode: 0
"""

COMPLETED_MSG = 'Restore COMPLETED.'

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_fbk: Path, tmp_fdb: Path, tmp_log: Path, capsys):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'gh_6785.zip', at = 'gh_6785.fbk')
    tmp_fbk.write_bytes(zipped_fbk_file.read_bytes())

    restore_code = -1

    # If the timeout expires, the child process will be killed and waited for.
    # The TimeoutExpired exception will be re-raised after the child process has terminated.
    tmp_log.unlink(missing_ok = True)
    try:
        p = subprocess.run([ act.vars['gbak'],
                            '-user', act.db.user, '-password', act.db.password,
                            '-rep', tmp_fbk, tmp_fdb,
                            '-st', 'tdrw', 
                            '-v', '-y', str(tmp_log)
                          ]
                          ,stderr = subprocess.STDOUT
                          ,timeout = MAX_THRESHOLD
                         )
        restore_code = p.returncode
        print( COMPLETED_MSG )

    except Exception as e:
        print(f'{e.__class__=}')
        # DO NOT: print(f'{e.errno=}') AttributeError: 'TimeoutExpired' object has no attribute 'errno'
        # Command '[...gbak...]'  timed out after 299.99996796250343 seconds
        print(e.__str__())
        
        if tmp_fdb.is_file():
            print('Size of [partially] restored DB: %d' % os.path.getsize(tmp_fdb))
        else:
            print('Target database does not exist!')

        tmp_fdb.unlink(missing_ok = True)

        if tmp_log.is_file():
            print('Check restore log:')
            with open(tmp_log, 'r') as f:
                print(f.read())
        else:
            print('Restore log does not exist!')

    act.expected_stdout = COMPLETED_MSG
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    if restore_code == 0:
        
        # Get FB log before validation, run validation and get FB log after it:
        with act.connect_server() as srv:

            fblog_1 = act.get_firebird_log()
            #with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
            #    srv.info.get_log()
            #    fblog_1 = srv.readlines()

            srv.database.repair(database = str(tmp_fdb), flags=SrvRepairFlag.CORRUPTION_CHECK)
            
            fblog_2 = act.get_firebird_log()
            #with act.connect_server(encoding=locale.getpreferredencoding()) as srv:
            #    srv.info.get_log()
            #    fblog_2 = srv.readlines()

            p_diff = re.compile('Validation finished: \\d+ errors, \\d+ warnings, \\d+ fixed')
            validation_result = ''
            for line in unified_diff(fblog_1, fblog_2):
                if line.startswith('+') and p_diff.search(line):
                    validation_result =line.strip().replace('\t', ' ')
                    break


            assert validation_result == '+ Validation finished: 0 errors, 0 warnings, 0 fixed'
            act.reset()
