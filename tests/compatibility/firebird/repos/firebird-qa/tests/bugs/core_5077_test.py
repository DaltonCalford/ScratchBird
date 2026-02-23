#coding:utf-8

"""
ID:          issue-5364
ISSUE:       5364
TITLE:       ISQL does not show encryption status of database
DESCRIPTION:
    Test uses Firebird built-in encryption plugin wich actually does encryption using trivial algorithm.
    Before running this test following prerequisites must be met:
        1. Files fbSampleDbCrypt.conf and libfbSampleDbCrypt.so/fbSampleDbCrypt.dll must present in the $FB_HOME/plugins folder;
        2. File fbSampleDbCrypt.conf must contain line: Auto = yes
        3. File $QA_HOME/pytest.ini must contain line with 'encryption' marker declaration.
    ### ACHTUNG ###
    Unfortunately, there is no files fbSampleDbCrypt.* in FB 3.x snapshots (at least for June-2022).
    One need to take both thiese files from any FB 4.x snapshot (they have backward compatibility).
    On FB 4.x these files can be found here: $FB_HOME/examples/prebuilt/plugins/
    ###############

    We open connection to DB and run 'ALTER DATABASE ENCRYPT.../DECRYPT'.
    One need to keep connection opened for several seconds in order to give encryption thread be fully completed.
    Duration of this delay depends on concurrent workload, usually it is almost zero.
    But in this test it can be tuned - see variable 'MAX_WAITING_ENCR_FINISH'.
    Immediately after launch encryption/decryption, we run isql and ask it to give result of 'SHOW DATABASE' command.
    If this output contains text 'Database [not] encrypted' and *not* contains phrase 'not complete' then we can assume
    that encryption/decryption thread completed. Otherwise we loop until such conditions will raise or timeout expired.

JIRA:        CORE-5077
FBTEST:      bugs.core_5077
NOTES:
    [06.06.2022] pzotov
    Checked on 4.0.1.2692, 3.0.8.33535 - both on Linux and Windows.
"""

import os
import time
import datetime as py_dt
from datetime import timedelta

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

###########################
###   S E T T I N G S   ###
###########################

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
enc_settings = QA_GLOBALS['encryption']

# ACHTUNG: this must be carefully tuned on every new host:
#
MAX_WAITING_ENCR_FINISH = int(enc_settings['MAX_WAIT_FOR_ENCR_FINISH_WIN' if os.name == 'nt' else 'MAX_WAIT_FOR_ENCR_FINISH_NIX'])
assert MAX_WAITING_ENCR_FINISH > 0

ENCRYPTION_PLUGIN = enc_settings['encryption_plugin'] # fbSampleDbCrypt
ENCRYPTION_KEY = enc_settings['encryption_key'] # Red

db = db_factory()
act = python_act('db')

@pytest.mark.version('>=3.0')
@pytest.mark.encryption
def test_1(act: Action, capsys):
    for m in ('encryption', 'decryption'):
        expected_stdout_console = f"""
            Expected: {m} status presents in the 'SHOW DATABASE' output.
        """

        with act.db.connect() as con:
            t1=py_dt.datetime.now()
            d1 = t1-t1
            sttm = 'alter database '  + ( f'encrypt with "{ENCRYPTION_PLUGIN}" key "{ENCRYPTION_KEY}"' if m == 'encryption' else 'decrypt' )
            try:
                con.execute_immediate(sttm)
                con.commit()
            except DatabaseError as e:
                print( e.__str__() )

            act.expected_stdout = ''
            act.stdout = capsys.readouterr().out
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()


            while True:
                t2=py_dt.datetime.now()
                d1=t2-t1
                if d1.seconds*1000 + d1.microseconds//1000 > MAX_WAITING_ENCR_FINISH:
                    break

                # Possible output:
                #     Database not encrypted
                #     Database encrypted, crypt thread not complete
                act.isql(switches=['-q'], input = 'show database;', combine_output = True)
                if m == 'encryption' and 'Database encrypted' in act.stdout or m == 'decryption' and 'Database not encrypted' in act.stdout:
                    if 'not complete' in act.stdout:
                        pass
                    else:
                        break
                act.reset()

            if d1.seconds*1000 + d1.microseconds//1000 <= MAX_WAITING_ENCR_FINISH:
                print(expected_stdout_console)
            else:
                print(f'BREAK ON TIMEOUT EXPIRATION: {m.upper()} took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {MAX_WAITING_ENCR_FINISH} ms.')

        act.expected_stdout = expected_stdout_console
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
