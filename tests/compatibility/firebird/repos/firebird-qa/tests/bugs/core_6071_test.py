#coding:utf-8

"""
ID:          issue-6321
ISSUE:       6321
TITLE:       Restore of encrypted backup of database with SQL dialect 1 fails
DESCRIPTION:
    Database is created in dialect 3, then encrypted and after this we apply 'gfix -sql_dialect 1' to it.
    Then we restore database and validate it.

    Confirmed bug on 4.0.0.1485 (build date: 11-apr-2019), got error on restore:
        SQL error code = -817
        Metadata update statement is not allowed by the current database SQL dialect 1

JIRA:        CORE-6071
FBTEST:      bugs.core_6071
NOTES:
    [15.06.2022] pzotov
    There is some bug on LINUX with gbak when we have to make backup/restore ENCRYPTED database:
    1. If we use names of encryption plugin and keyholder WITHOUT quotes (i.e. ENCRYPTION_PLUGIN = 'fbSampleDbCrypt' and ENCRYPTION_HOLDER = 'fbSampleKeyHolder')
        then backup works fine but *restore* fails with following message:
        > act.gbak(switches=['-c', '-KEYHOLDER', ENCRYPTION_HOLDER, str(tmp_fbk), str(tmp_res) ])
        gbak: ERROR:unsuccessful metadata update
        gbak: ERROR:    ALTER DATABASE failed
        gbak: ERROR:    Crypt plugin FBSAMPLEDBCRYPT failed to load

    2. If we enclose names into double quotes then *backup* fails like this:
        > act.gbak(switches=['-b', '-KEYHOLDER', '"'+ENCRYPTION_HOLDER+'"', '-crypt', '"'+ENCRYPTION_PLUGIN+'"', str(act.db.dsn), str(tmp_fbk)])
        gbak: ERROR:Key holder plugin "fbSampleKeyHolder" failed to load
        gbak:Exiting before completion due to errors

    Until this weird error will be fixed, this test must be run only on WINDOWS.
    Sent example with bug reproducing to Alex et al, 16-jun-2022 01:04. Waiting for reply.
    Checked on 4.0.1.2692, 5.0.0.509.
"""
import os
import re
import datetime as py_dt
from datetime import timedelta
import time

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError
from pathlib import Path


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

ENCRYPTION_HOLDER = enc_settings['encryption_holder'] # fbSampleKeyHolder

db = db_factory()

#act = python_act('db', substitutions=[('\\d+ BYTES WRITTEN', '')])
act = python_act('db')

tmp_fbk = temp_file( filename = 'tmp_core_6071_encrypted.fbk')
tmp_res = temp_file( filename = 'tmp_core_6071_restored.fdb')
tmp_log = temp_file( filename = 'tmp_core_6071_bkup_rest.log')

@pytest.mark.encryption
@pytest.mark.version('>=4.0')
@pytest.mark.platform('Windows')
def test_1(act: Action, tmp_fbk: Path, tmp_res: Path, tmp_log: Path, capsys):

    encryption_started = False
    encryption_finished = False
    with act.db.connect() as con:

        t1=py_dt.datetime.now()
        d1 = t1-t1
        sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}" key "{ENCRYPTION_KEY}"'
        try:
            con.execute_immediate(sttm)
            con.commit()
            encryption_started = True
        except DatabaseError as e:
            # -ALTER DATABASE failed
            # -Crypt plugin fbSampleDbCrypt failed to load
            #  ==> no sense to do anything else, encryption_started remains False.
            print( e.__str__() )

        while encryption_started:
            t2=py_dt.datetime.now()
            d1=t2-t1
            if d1.seconds*1000 + d1.microseconds//1000 > MAX_WAITING_ENCR_FINISH:
                print(f'TIMEOUT EXPIRATION: encryption took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {MAX_WAITING_ENCR_FINISH} ms.')
                break

            # Possible output:
            #     Database not encrypted
            #     Database encrypted, crypt thread not complete
            act.isql(switches=['-q'], input = 'show database;', combine_output = True)
            if 'Database encrypted' in act.stdout:
                if 'not complete' in act.stdout:
                    pass
                else:
                    encryption_finished = True
                    break
            act.reset()
    
    if encryption_finished:
        act.expected_stdout = ''
        act.expected_stderr = ''

        # All following command must NOT issue anything.
        # Any output must be considered as errror:
        #
        act.gfix(switches=['-sql_dialect', '1', act.db.dsn])
        act.gbak(switches=['-b', '-KEYHOLDER', ENCRYPTION_HOLDER, '-crypt', ENCRYPTION_PLUGIN, str(act.db.dsn), str(tmp_fbk)])
        act.gbak(switches=['-c', '-KEYHOLDER', ENCRYPTION_HOLDER, str(tmp_fbk), str(tmp_res) ])
        act.gfix(switches=['-v', '-full', tmp_res])
