#coding:utf-8

"""
ID:          issue-7917
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7917
TITLE:       Hang in a case of error when sweep thread is attaching to database
DESCRIPTION:
    Test uses preliminary created alias with KeyHolderPlugin that points to special configuration with name 'KH2'.
    This configuration makes crypt plugin accept key ONLY from client app (in contrary to some other encryption-related tests).
    DB-level triggers (ON CONNECT, ON DISCONNECT) are created for logging appropriate events (see table 'att_log').
    Then we run special SQL that uses autonomous transactions and ES/EDS.
    This script will hang because of update-conflict after some number of transactions (see TX_NUMBER_BEFORE_HANG).
    We run this script asynchronously, then reduce sweep interval to value SWP_INTERVAL_TO_CHECK that is less than TX_NUMBER_BEFORE_HANG,
    change DB state to full shutdown and return it online.
    At this point any connection to DB will fire AUTO SWEEP (normally this can be seen in firebird.log as 'Sweep is started by SWEEPER').
    We run ISQL that queries 'att_log' table and causes AUTO SWEEP. That ISQL has to normally detach from DB and we must see its results.
NOTES:
    [28.12.2023] pzotov
    1. To make crypt plugin accept key only from client app, $FB_HOME/plugins.conf must contain following lines:
    ==================
    Plugin = KH2 {
        Module = $(dir_plugins)/fbSampleKeyHolder
        RegisterName = fbSampleKeyHolder
        Config = KH2
    }
    Config = KH2 {
        Auto = false
    }
    ==================
    QA-scenario (.bat and .sh) must add in advance content of $QA_ROOT/files/qa-plugins-supplement.conf to $FB_HOME/plugins.conf.

    2. Demo-plugin (fbSampleKeyHolder) can transfer key over network only for default key which has no-name.
       Because of this, command 'alter database encrypt with <plugin>' has no '... key <key>' tail.
       See letter from Alex, 15.12.2023 16:16

    3. In case of regression caused by that bug, we have to be ready that FB will hang on this test!

    Great thanks to Alex for suggestions (discussion started 13.12.2023 13:18).

    Confirmed bug on 6.0.0.173.
    Checked on 6.0.0.175, 5.0.0.1305, 4.0.5.3049 - but currenlyt only SuperServer works fine.
    Classic has the same problem. Sent report to Alex, 28.12.2023 13:10.

    On 3.0.12.33726 error raises:
        unsuccessful metadata update / -ALTER DATABASE failed / -Missing correct crypt key / -Plugin fbSampleDbCrypt: / -Crypt key not set
    This problem not [yet] investogated.
"""
import os
import time
import datetime as py_dt
import subprocess
import locale
import re
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

REQUIRED_ALIAS = 'tmp_gh_7917_alias'

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

SWP_INTERVAL_TO_CHECK = 100
TX_NUMBER_BEFORE_HANG = SWP_INTERVAL_TO_CHECK + 10

MAX_WAIT_FOR_ISQL_TERMINATE=11

db = db_factory(filename = '#' + REQUIRED_ALIAS, do_not_drop = True)

act = python_act('db', substitutions = [('^((?!(ISQL|Attributes)).)*$', ''), ('[ \t]+', ' '), ('TCPv(4|6)$', 'TCP')])
#act = python_act('db')

tmp_sql_file = temp_file('tmp_7917.sql')
tmp_log_file = temp_file('tmp_7917.log')

#-----------------------------------------------------------------------

def run_encr_decr(act: Action, mode, max_wait_encr_thread_finish, capsys):
    if mode == 'encrypt':
        # See letter from Alex, 15.12.2023 16:16 demo-plugin can not transfer named key over network.
        # Because of that, we have to use following command WITHOUT adding 'key "{ENCRYPTION_KEY}"':
        # ::: NB ::: One need to be sure that $FB_HOME/plugins.conf contains following lines:
        # Plugin = KH2 {
        #     Module = $(dir_plugins)/fbSampleKeyHolder
        #     RegisterName = fbSampleKeyHolder
        #     Config = KH2
        # }
        #  Config = KH2 {
        #     Auto = false
        # }
        # Otherwise error will raise:
        # unsuccessful metadata update
        # -ALTER DATABASE failed
        # -Missing database encryption key for your attachment
        # -Plugin fbSampleDbCrypt:
        # -Crypt key not set
        #
        alter_db_sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}"'
        wait_for_state = 'Database encrypted'
    elif mode == 'decrypt':
        alter_db_sttm = 'alter database decrypt'
        wait_for_state = 'Database not encrypted'


    e_thread_started = False
    e_thread_finished = False
    with act.db.connect() as con:

        t1=py_dt.datetime.now()
        d1 = t1-t1
        try:
            con.execute_immediate(alter_db_sttm)
            con.commit()
            e_thread_started = True
        except DatabaseError as e:
            print( e.__str__() )

        while e_thread_started:
            t2=py_dt.datetime.now()
            d1=t2-t1
            if d1.seconds*1000 + d1.microseconds//1000 > max_wait_encr_thread_finish:
                print(f'TIMEOUT EXPIRATION. Mode="{mode}" took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {max_wait_encr_thread_finish} ms.')
                break

            # Possible output:
            #     Database [not] encrypted
            #     Database encrypted, crypt thread not complete
            act.isql(switches=['-q'], input = 'show database;', combine_output = True)
            if wait_for_state in act.stdout:
                if 'not complete' in act.stdout:
                    pass
                else:
                    e_thread_finished = True
                    break
            act.reset()

        act.expected_stdout = ''
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

    assert e_thread_finished


#-----------------------------------------------------------------------

@pytest.mark.es_eds
@pytest.mark.encryption
@pytest.mark.version('>=4.0.5')
@pytest.mark.platform('Windows')
def test_1(act: Action, tmp_sql_file: Path, tmp_log_file: Path, capsys):

    '''
    with act.db.connect() as con:
        if act.vars['server-arch'] == 'SuperServer':
            pass
        else:
            pytest.skip("Currently fixed only for SuperServer. Temporary SKIPPED.")
    '''

    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_7598_alias = $(dir_sampleDb)/qa/tmp_gh_7598.fdb
                # - then we extract filename: 'tmp_gh_7598.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    #----------------------------------------------------------------

    run_encr_decr(act, 'encrypt', MAX_WAITING_ENCR_FINISH, capsys)

    test_script = f"""
        set bail on;
        create table att_log(
             att_prot varchar(15)
            ,who_ami varchar(31) default current_user
            ,att_id bigint default current_connection
            ,trn_id bigint default current_transaction
            ,evt_time time default 'now'
            ,evt_name varchar(20)
            ,swp_interval int
        );
        set term ^;
        create procedure sp_fill_dblevel_log(a_evt_name type of column att_log.evt_name) as
            declare v_swp_interval int;
            declare v_protocol type of column att_log.att_prot;
        begin
            insert into att_log(
                 att_prot
                ,evt_name
            ) values (
                 rdb$get_context('SYSTEM', 'NETWORK_PROTOCOL')
                ,:a_evt_name
            );

        end
        ^
        create or alter trigger trg_detach on disconnect as
        begin
            execute procedure sp_fill_dblevel_log('detach');
        end
        ^
        create or alter trigger trg_attach on connect as
        begin
            execute procedure sp_fill_dblevel_log('attach');
        end
        ^
        set term ;^
        commit;
        
        recreate table test(s varchar(36) unique);
        insert into test(s) values('LOCKED_FOR_PAUSE');
        commit;

        set transaction read committed WAIT;

        update test set s = s where s = 'LOCKED_FOR_PAUSE';

        set term ^;
        execute block as
            declare n int = {TX_NUMBER_BEFORE_HANG};
            declare v_role varchar(31);
        begin
            while (n > 0) do
                in autonomous transaction do
                insert into test(s) values( rpad('', 36, uuid_to_char(gen_uuid()) ) )
                returning :n-1 into n;

            v_role = left(replace( uuid_to_char(gen_uuid()), '-', ''), 31);

            begin
                execute statement ('update /* ES/EDS */ test set s = s where s = ?') ('LOCKED_FOR_PAUSE')
                on external
                    'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
                    as user 'SYSDBA' password 'masterkey' role v_role
                with autonomous transaction;
            when any do
                begin
                end
            end

        end
        ^
        set term ;^
        set heading off;
        select '-- shutdown me now --' from rdb$database;
    """
    
    tmp_sql_file.write_text(test_script)

    #----------------------------------------------------------------

    # Reduce sweep interval to small value (that must be less than SQL_HANG_AFTER_TX_CNT):
    #
    act.gfix(switches=['-h', f'{SWP_INTERVAL_TO_CHECK}', act.db.dsn], combine_output = True, io_enc = locale.getpreferredencoding())

    with open(tmp_log_file, 'w') as f:
        # Launch ISQL which will hang because update conflict.
        # This ISQl will be 'self-terminated' further because we will change DB state to full shutdown:
        #
        p_handed_isql = subprocess.Popen([act.vars['isql'], '-nod', '-i', str(tmp_sql_file),
                                       '-user', act.db.user,
                                       '-password', act.db.password, act.db.dsn],
                                      stdout = f,
                                      stderr = subprocess.STDOUT)

        # Let ISQL time to establish connection and fall in hanging state:
        time.sleep(3)

        try:
            act.gfix(switches=['-shut', 'full', '-force', '0', act.db.dsn], combine_output = True, io_enc = locale.getpreferredencoding())
        finally:
            p_handed_isql.terminate()

        p_handed_isql.wait(MAX_WAIT_FOR_ISQL_TERMINATE)
        if p_handed_isql.poll() is None:
            print(f'Hanged ISQL process WAS NOT terminated in {MAX_WAIT_FOR_ISQL_TERMINATE} second(s).!')
        else:
            print(f'Hanged ISQL process terminated with retcode = {p_handed_isql.poll()}')

    # Result: log of hanged ISQL must contain now:
    # Statement failed, SQLSTATE = 08003
    # connection shutdown
    # -Database is shutdown.    
    
    act.gfix(switches=['-online', act.db.dsn], combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == ''
    act.reset()

    # Must show: Attributes encrypted, plugin {ENCRYPTION_PLUGIN} - without 'shutdown'!
    act.gstat(switches=['-h'])
    print(act.stdout)

    #act.stdout = capsys.readouterr().out
    #assert act.clean_stdout == act.clean_expected_stdout
    #act.reset()


    #----------------------------------------------------------------

    TEST_QUERY = 'select att_prot,who_ami,evt_name from att_log order by trn_id'
    final_sql = f"""
        set count on;
        set list on;
        set echo on;
        {TEST_QUERY};
        commit;
        connect '{act.db.dsn}';
        drop database;
        quit;
    """
    tmp_sql_file.write_text(final_sql)

    with open(tmp_log_file, 'w') as f:
        # Explained by Alex, letter 13-dec-2023 13:18.
        # Following ISQL will create attach that provokes AUTO SWEEP (because Next - OST now greater than SWP_INTERVAL_TO_CHECK).
        # Problem raised when other attachments were prohibited to use encryption key (and this is default behaviour).
        # Before fix, SWEEEP was not allowed to use key from this ISQL-attachment.
        # Following message was added in firebird.log: "Automatic sweep error /Missing database encryption key for your attachment"
        # But despite problem with establishing connection by SWEEP, its thread already created appropriate lock at that point.
        # As result, engine remained in wrong state after this: existied attachments could not be closed.
        # Also, FB process could not be normally stopped.
 
        MAX_WAIT_AUTO_SWEEP_FINISH = 3
        p_chk_sql = subprocess.Popen( [ act.vars['isql'],
                                      '-nod', '-i', str(tmp_sql_file),
                                      '-user', act.db.user,
                                      '-password', act.db.password,
                                      act.db.dsn
                                    ],
                                    stdout = f,
                                    stderr = subprocess.STDOUT,
                                  )

        # If the process does not terminate after timeout seconds, raise a TimeoutExpired exception.
        # It is safe to catch this exception and retry the wait.
        try:
            p_chk_sql.wait(timeout = MAX_WAIT_AUTO_SWEEP_FINISH)
        except subprocess.TimeoutExpired as e:
            print(f'Could not obtain result for {MAX_WAIT_AUTO_SWEEP_FINISH} seconds:')
            print(e.__str__())

        p_chk_sql.terminate()
        p_chk_sql.wait(MAX_WAIT_FOR_ISQL_TERMINATE)

        # Check if child process has terminated. Set and return returncode attribute. Otherwise, returns None.
        if p_chk_sql.poll() is None:
            print(f'Final ISQL process WAS NOT terminated in {MAX_WAIT_FOR_ISQL_TERMINATE} second(s).!')
        else:
            print(f'Final ISQL process terminated')
            #print(f'Final ISQL process terminated with retcode = {p_chk_sql.poll()}')


    # do NOT put here this:
    #run_encr_decr(act, 'decrypt', MAX_WAITING_ENCR_FINISH, capsys)
    # - otherwise pytest will not return control
    
    with open(tmp_log_file, 'r') as f:
        for line in f:
            if line.strip():
                print('final ISQL log:',line)

    act.expected_stdout = f"""
        Hanged ISQL process terminated with retcode = 1
        Attributes encrypted, plugin {ENCRYPTION_PLUGIN}

        Final ISQL process terminated
        final ISQL log: {TEST_QUERY};
        final ISQL log: ATT_PROT                        TCP
        final ISQL log: WHO_AMI                         {act.db.user.upper()}
        final ISQL log: EVT_NAME                        attach

        final ISQL log: ATT_PROT                        TCP
        final ISQL log: WHO_AMI                         {act.db.user.upper()}
        final ISQL log: EVT_NAME                        detach

        final ISQL log: Records affected: 2

        final ISQL log: commit;
        final ISQL log: drop database;
        final ISQL log: quit;
    """

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # NB! We have to decrypt database now. Otherwise teardown will fail with:
    #   firebird.driver.types.DatabaseError: Missing database encryption key for your attachment
    #   -Plugin fbSampleDbCrypt:
    #   -Crypt key not set
    #run_encr_decr(act, 'decrypt', MAX_WAITING_ENCR_FINISH, capsys)
