#coding:utf-8

"""
ID:          issue-5995
ISSUE:       5995
TITLE:       Connection to server may hang when working with encrypted databases over non-TCP protocol
DESCRIPTION:
    Test implemented only to be run on Windows.
    Folder %FIREBIRD_HOME%/plugins/ must have files fbSampleKeyHolder.conf and fbSampleKeyHolder.dll which should be
    copied there from %FIREBIRD_HOME%/examples/prebuilt/plugins/.
    NB! These files ABSENT in FB 3.x but they can be taken from FB 4.x snapshot.
    File fbSampleKeyHolder.conf must have following lines:
        Auto = true
        KeyRed=111

    If we encrypt database and then make file fbSampleKeyHolder.conf EMPTY then usage of XNET and WNET protocols became
    impossible before this ticket was fixed.
    Great thanks to Alex for suggestions.
JIRA:        CORE-5730
FBTEST:      bugs.gh_5995
NOTES:
    [03.06.2024] pzotov
    Confirmed bug on 3.0.1.32609, 4.0.0.853: ISQL hangs on attempt to connect to database when file plugins/keyholder.conf is empty.
    Checked on 6.0.0.366, 5.0.1.1411, 4.0.5.3103 (all of them were checked for ServerMode = SS and CS).

    ATTENTION: 3.x raises different SQLSTATE and error text, depending on ServerMode!
    For 3.x value of SQLSTATE and error text depends on Servermode.
    On Classic FB 3.x output will be almost like in FB 4.x+:
       Statement failed, SQLSTATE = 08004
       Missing correct crypt key
       -Plugin fbSampleDbCrypt:
       -Crypt key Red not set
       -IProvider::attachDatabase failed when loading mapping cache
    On Super FB 3.x output is:
       Statement failed, SQLSTATE = HY000
       Missing correct crypt key
       -Plugin fbSampleDbCrypt:
       -Crypt key Red not set
    Because of that, it was decided not to check FB 3.x as this version soon will be considered as obsolete.
"""

import shutil
import locale
import re
import time
import platform
import subprocess

import datetime as py_dt
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError, DbInfoCode, NetProtocol

import time

###########################
###   S E T T I N G S   ###
###########################

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
enc_settings = QA_GLOBALS['encryption']

# ACHTUNG: this must be carefully tuned on every new host:
#
MAX_WAITING_ENCR_FINISH = int(enc_settings['MAX_WAIT_FOR_ENCR_FINISH_WIN'])
assert MAX_WAITING_ENCR_FINISH > 0

ENCRYPTION_PLUGIN = enc_settings['encryption_plugin'] # fbSampleDbCrypt
ENCRYPTION_KEY = enc_settings['encryption_key'] # Red

db = db_factory()
act = python_act('db', substitutions = [('After line \\d+.*', ''),('[ \t]+', ' ')])

kholder_cfg_bak = temp_file('fbSampleKeyHolder.bak')
tmp_sql = temp_file('tmp_5995.sql')
tmp_log = temp_file('tmp_5995.log')

#-----------------------------------------------------------------------

def run_encr_decr(act: Action, mode, max_wait_encr_thread_finish, capsys):
    
    assert mode in ('encrypt', 'decrypt')

    if mode == 'encrypt':
        alter_db_sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}" key "{ENCRYPTION_KEY}"'
        wait_for_state = 'Database encrypted'
    elif mode == 'decrypt':
        alter_db_sttm = 'alter database decrypt'
        wait_for_state = 'Database not encrypted'

    e_thread_finished = False

    d1 = py_dt.timedelta(0)
    with act.db.connect() as con:
        t1=py_dt.datetime.now()
        try:
            d1 = t1-t1
            con.execute_immediate(alter_db_sttm)
            con.commit()
            # Pattern to check for completed encryption thread:
            completed_encr_pattern = re.compile(f'Attributes\\s+encrypted,\\s+plugin\\s+{ENCRYPTION_PLUGIN}', re.IGNORECASE)
            while not e_thread_finished:
                t2=py_dt.datetime.now()
                d1=t2-t1
                if d1.seconds*1000 + d1.microseconds//1000 > max_wait_encr_thread_finish:
                    break
    
                #############################################
                ###   C H E C K    G S T A T    A T T R.  ###
                #############################################
                # Invoke 'gstat -h' and read its ouput.
                # Encryption can be considered as COMPLETED when we will found:
                # "Attributes              encrypted, plugin fbSampleDbCrypt"
                #
                act.gstat(switches=['-h'])
                for line in act.stdout.splitlines():
                    if mode == 'encrypt' and completed_encr_pattern.match(line.strip()):
                        e_thread_finished = True
                    if mode == 'decrypt' and 'Attributes' in line and not completed_encr_pattern.search(line.strip()):
                        e_thread_finished = True
                    if e_thread_finished:
                        break

                time.sleep(0.5)
                    
        except DatabaseError as e:
            print( e.__str__() )

    assert e_thread_finished, f'TIMEOUT EXPIRATION: {mode=} took {d1.seconds*1000 + d1.microseconds//1000} ms which greater than {max_wait_encr_thread_finish=} ms'

#-----------------------------------------------------------------------

@pytest.mark.encryption
@pytest.mark.version('>=4.0')
@pytest.mark.platform('Windows')
def test_1(act: Action, kholder_cfg_bak: Path, tmp_sql: Path, tmp_log: Path, capsys):
    kholder_cfg_file = act.vars['home-dir'] / 'plugins' / 'fbSampleKeyHolder.conf'
    shutil.copy2(kholder_cfg_file, kholder_cfg_bak)
    finish_encryption = False

    protocols_list = [ NetProtocol.INET, NetProtocol.XNET, ]
    if act.is_version('<5'):
        protocols_list.append(NetProtocol.WNET)

    expected_output = actual_output = test_sql = ''
    try:
        run_encr_decr(act, 'encrypt', MAX_WAITING_ENCR_FINISH, capsys)
        finish_encryption = True
        with open(kholder_cfg_file,'w') as f:
            pass
        
        for protocol_name in protocols_list:
            conn_str = f"connect {protocol_name.name.lower()}://{act.db.db_path} user {act.db.user} password '{act.db.password}'"
            test_sql = f"""
                set list on;
                set bail on;
                set echo on;
                {conn_str};
                select mon$remote_protocol from mon$attachments where mon$attachment_id = current_connection;
            """
            tmp_sql.write_text(test_sql)
            
            with open(tmp_log, 'w') as f_log:
                # ISQL-4.x must issue:
                #   Statement failed, SQLSTATE = 08004
                #   Missing database encryption key for your attachment
                #   -Plugin fbSampleDbCrypt:
                #   -Crypt key Red not set
                # Before fix, ISQL hanged on CONNECT, thus we have to use timeout here!
                #
                p = subprocess.run( [ act.vars['isql'],
                                      '-q',
                                      '-i', str(tmp_sql)
                                    ], 
                                    stdout = f_log, stderr = subprocess.STDOUT,
                                    timeout = 3
                                  )
       
            actual_output += tmp_log.read_text()

            expected_output += f"""
                {conn_str};
                Statement failed, SQLSTATE = 08004
                Missing database encryption key for your attachment
                -Plugin {ENCRYPTION_PLUGIN}:
                -Crypt key {ENCRYPTION_KEY} not set
            """
    
    except Exception as e:
        actual_output += test_sql  + '\n' + e.__str__()
    finally:
        shutil.copy2(kholder_cfg_bak, kholder_cfg_file)
        if finish_encryption:
            run_encr_decr(act, 'decrypt', MAX_WAITING_ENCR_FINISH, capsys)

    act.expected_stdout = expected_output
    act.stdout = actual_output # capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
