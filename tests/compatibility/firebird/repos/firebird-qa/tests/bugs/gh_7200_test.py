
#coding:utf-8

"""
ID:          issue-7200
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7200
TITLE:       DROP DATABASE lead FB to hang if it is issued while DB encrypting/decrypting is in progress
DESCRIPTION:
    Test does exactly what is described in the ticket: creates DB, does not add anydata to it, runs encryption
    ('alter database encrypt ...' using fbSampleDbCrypt plugin) and *immediately* attempts to drop this DB.
    Only encryption is checked (i.e. no decrypt).

    The key issue: no delay must be added between encryption command and drop DB attempt.
    Problem can be reproduced on snapshot 4.0.1.2692 (at least on Windows): FB hangs.
    After this bug was fixed, client received exception:
        SQLSTATE = 42000 / unsuccessful metadata update / -object DATABASE is in use
    - and terminates itself (without need to do this forcibly via subprocess terminate() call).
    
    But it must be remembered that encryption command works in asynchronous mode, so it can be the case when
    DROP database starts to execute *before* encryption thread, as it was noted by Alex:
        https://github.com/FirebirdSQL/firebird/issues/7200#issuecomment-1147672310
    This means that client can get NO exception at all and database will be 'silently' dropped eventually.
    Because of this, we must *not* wait for exception delivering to client and check its presense.
    Rather, we must only to ensure that client can make some FURTHER actions after DROP command, e.g.
    it can try to create ANOTHER database.
    This caused us to use TWO databases for this test: one for purpose to check ability to DROP it and second
    to ensure that client does not hang and can do something after finish with first DB (see 'act1' and 'act2').
    Both databases are created and dropped 'manually', i.e. w/o fixture.

    Summary, the test should check two things:
    1) self-termination of client process for reasonable time (see setting MAX_WAIT_FOR_ISQL_TERMINATE);
    2) ability of client to create another DB after the one that was dropped.
    Code related to these requirements operates with 'EXPECTED_MSG_1' and 'EXPECTED_MSG_2' variables: we check in
    stdout presense of the text which they store.

NOTES:
    [03.03.2023] pzotov
    1. Settings for encryption are taken from act.files_dir/test_config.ini file.
    2. We have to avoid usage of act_tmp.db.drop_database() because it suppresses any occurring exception.

    Checked on Linux: 6.0.0.660; 5.0.3.1628; 4.0.6.3190 (SS and CS).
    Checked on Windows: 6.0.0.658; 5.0.3.1624; 4.0.6.3189 (SS).
"""

from pathlib import Path
import subprocess
import time

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

#########################
###  S E T T I N G S  ###
#########################

MAX_WAIT_FOR_ISQL_TERMINATE = 5

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings from $QA_ROOT/test_config.ini:
enc_settings = QA_GLOBALS['encryption']

encryption_plugin = enc_settings['encryption_plugin'] # fbSampleDbCrypt
encryption_holder  = enc_settings['encryption_holder'] # fbSampleKeyHolder
encryption_key = enc_settings['encryption_key'] # Red

EXPECTED_MSG_1 = f'EXPECTED: ISQL process has terminated for less than {MAX_WAIT_FOR_ISQL_TERMINATE} second(s).'
EXPECTED_MSG_2 = "EXPECTED: script could continue after 'DROP DATABASE'"

db1 = db_factory(filename = 'tmp_gh_7200.tmp.fdb', do_not_create = True, do_not_drop = True)
db2 = db_factory(filename = 'tmp_gh_7200.tmp2.fdb', do_not_create = True, do_not_drop = True)

act1 = python_act('db1', substitutions = [('[ \t]+', ' '), ('^((?!(EXPECTED:|ISQL_LOG:)).)*$', '')])
act2 = python_act('db2')

tmp_run_encrypt_sql = temp_file(filename = 'tmp_gh_7200-run-encr.sql')
tmp_run_encrypt_log = temp_file(filename = 'tmp_gh_7200-run-encr.log')

@pytest.mark.encryption
@pytest.mark.version('>=4.0.1')
def test_1(act1: Action, act2: Action, tmp_run_encrypt_sql: Path, tmp_run_encrypt_log: Path, capsys):


    act1.db.db_path.unlink(missing_ok = True)
    act2.db.db_path.unlink(missing_ok = True)

    sttm = f"""
        create database '{act1.db.dsn}';
        alter database encrypt with "{encryption_plugin}" key "{encryption_key}";
        drop database;
        rollback;
        create database '{act2.db.dsn}';
        set headinf off;
        select iif(  mon$database_name containing '{act2.db.db_path}'
                    ,q'[{EXPECTED_MSG_2}]'
                    ,'UNEXPECTED value of mon$database_name = ' || mon$database_name
                  ) as result_for_check from mon$database
        ;
    """
    tmp_run_encrypt_sql.write_bytes(sttm.encode('utf-8'))

    with tmp_run_encrypt_log.open('w') as f_log:
        p_isql_encr = subprocess.Popen( [ act1.vars['isql'],
                                '-q',
                                '-user', act1.db.user,
                                '-password', act1.db.password,
                                '-i', tmp_run_encrypt_sql
                              ],
                              stdout = f_log, stderr = subprocess.STDOUT
                            )

        time.sleep(1)
        if p_isql_encr:
            try:
                p_isql_encr.wait(MAX_WAIT_FOR_ISQL_TERMINATE)
                print(EXPECTED_MSG_1)
                with tmp_run_encrypt_log.open('r') as f_log:
                    isql_log = f_log.read()
                if EXPECTED_MSG_2 in isql_log:
                    print(EXPECTED_MSG_2)
                else:
                    # Statement failed, SQLSTATE = 42000
                    # unsuccessful metadata update
                    # -object DATABASE is in use
                    for line in isql_log.splitlines():
                        if line.split():
                            print(f'ISQL_LOG: {line}')

            except subprocess.TimeoutExpired:
                p_isql_encr.terminate()
                print(f'UNEXPECTED: ISQL process WAS NOT completed in {MAX_WAIT_FOR_ISQL_TERMINATE=} second(s) and was forcibly terminated.')

    try:
        act1.db.db_path.unlink(missing_ok = True)
    except PermissionError as e:
        print(f'UNEXPECTED: Could not remove file {act1.db.db_path}')
        print(f'UNEXPECTED: {e.__class__=}, {e.errno=}')

    act2.db.db_path.unlink(missing_ok = True)

    act1.expected_stdout = f"""
        {EXPECTED_MSG_1}
        {EXPECTED_MSG_2}
    """
    act1.stdout = capsys.readouterr().out
    assert act1.clean_stdout == act1.clean_expected_stdout
