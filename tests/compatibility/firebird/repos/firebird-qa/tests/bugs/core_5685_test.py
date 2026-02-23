#coding:utf-8

"""
ID:          issue-5951
ISSUE:       5951
TITLE:       Sometime it is impossible to cancel/kill connection executing external query
DESCRIPTION:
    Problem did appear when host "A" established connection to host "B" but could not get completed reply from this "B".
    This can be emulated by following steps:
    1. We establich new remote connection to the same database using EDS mechanism and supply completely new ROLE to force new attachment be created;
    2. Within this EDS we do query to selectable procedure (with name 'sp_unreachable') which surely will not produce any result.
       Bogon IP '192.0.2.2' is used in order to make this SP hang for sufficient time (on Windows it is about 20, on POSIX - about 44 seconds).
    Steps 1 and 2 are implemented by asynchronous call of ISQL: we must have ability to kill its process after.
    When this 'hanging ISQL' is launched, we wait 1..2 seconds and run one more ISQL, which has mission to KILL all attachments except his own.
    This ISQL session is named 'killer', and it writes result of actions to log.
    This "killer-ISQL" does TWO iterations with the same code which looks like 'select ... from mon$attachments' and 'delete from mon$attachments'.
    First iteration must return data of 'hanging ISQL' and also this session must be immediately killed.
    Second iteration must NOT return any data - and this is main check in this test.

    For builds which had bug (before 25.12.2017) one may see that second iteration STILL RETURNS the same data as first one:
    ====
      ITERATION_NO                    1
      HANGING_ATTACH_CONNECTION       1
      HANGING_ATTACH_PROTOCOL         TCP
      HANGING_STATEMENT_STATE         1
      HANGING_STATEMENT_BLOB_ID       0:3
      select * from sp_get_data
      Records affected: 1

      ITERATION_NO                    2
      HANGING_ATTACH_CONNECTION       1
      HANGING_ATTACH_PROTOCOL         TCP
      HANGING_STATEMENT_STATE         1
      HANGING_STATEMENT_BLOB_ID       0:1
      select * from sp_get_data
      Records affected: 1
    ====
    (expected: all fields in ITER #2 must be NULL)
JIRA:        CORE-5685
NOTES:
    [03.02.2026] pzotov
        ::: ACHTUNG ::: In case of using VPN one need to add IP = 192.0.2.1 to exclusions list.
        TCP-request to this IP must be direct (i.e. bypass VPN).
        Otherwise script finishes INSTANTLY with 'connection rejected by remote interface'
        (instead of hang for ~20 seconds)

        Despite that 'killer' script completes relatively fast (for ~2...3s), database remains opened
        for approx 20s. Attempt to close DB file using 'gfix -shut' leads to SAME waiting ~20s.
        If we quit from this test without dropping such DB then next test_*.py (from QA set) will fail
        on attempt to create DB with same name ('object in use' will raise).
        Similar problem raises if we use db_factory with random file name because pytest attempts to
        drop entire directory defined as temporary storage for its run.
        Also, this problem will raise if this test outcome is verified multiple times via LOOP.

        Probably, this can be solved by set 'TcpMaxDataRetransmissions' parameter to small value in
        HKEY_LOCAL_MACHINE/System/CurrentControlSet/Services/Tcpip/Parameters (default = 5)
        But if we reduce this value then isql will not hang for valuable time and test will fail.
        Explanation by Vlad about to synchronous behaviour of connection establishing:
        http://www.ibaseforum.ru/viewtopic.php?f=8&t=4859&p=31026 // (rus, 2009)

        Because of that, test creates DB in _OS_ temp directory rather than in Pytest temp folder.
        Name of DB matches to the pattern: 'core-5685.<random_str>.tmp'
        Special code at the start tries to make cleanup of OS diretory with removing all such files
        (and suppressing exception like 'PermissionError: [WinError 32] The process cannot access...')

        Confirmed bug on 4.0.0.483; 3.0.2.32703
        Confirmed fix in 4.0.0.840; 3.0.3.32900.
        Checked on Windows (all SS/CS): 6.0.0.1403; 5.0.4.1748; 4.0.7.3243; 3.0.14.33829
        Checked on Linux: 6.0.0.1403 (SS/CS). Removed mark that prohibits exectuion on Linux.
"""
import os
import platform
import pytest
import re
import random
import string
import time
import datetime as py_dt
import locale
import shutil
import subprocess
from pathlib import Path
import tempfile

from firebird.qa import *
from firebird.driver import tpb, Isolation, DatabaseError, core as fb_core

for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

##########################
###   S E T T I N G S  ###
##########################
MAX_WAIT_FOR_WORKER_START_MS = 20000
MAX_WAIT_FOR_KILLER_FINISH_MS = 15000
BOGON_IP_ADDRESS = '192.0.2.1'
VPN_WARNING_MSG = f"\n::: NOTE ::: If you use VPN then add IP={BOGON_IP_ADDRESS} to the exclusions list."

# cleanup temp dir: remove all files that could remain from THIS test previous runs:
# -----------------
db_test_path = Path(tempfile.gettempdir())
db_test_ptrn = 'core-5685.*.tmp'
for file_path in db_test_path.glob(db_test_ptrn):
    if file_path.is_file(): # Ensure it's a file and not a directory
        try:
            file_path.unlink(missing_ok = True)
        except OSError as e:
            pass

# ::: NB ::: We have to use DB in OS temp dir rather than in folder defined as Pytest --basetemp!
# Otherwise loop of this test will fail at 2nd iter with 'object in use' because database remains opened for ~20 seconds
# after 'killer' connection completes. Attempt to close DB faster using 'gfix -shut...' DOES NOT HEELP!
#
DB_FILE_NAME = db_test_path / ( 'core-5685.' + ''.join(random.choices(string.ascii_lowercase + string.digits, k=8)) + '.tmp')
TEST_DB_DSN = f'localhost:{DB_FILE_NAME}'

db = db_factory()

# 09.07.2024. Added subs for 'port detached' message that raises in dev build.
substitutions = [  
                     (r'After line.*', 'After line <N>')
                    ,(r'line(:)\s+\d+.*', '')
                    ,(r'.*_BLOB_ID.*', '')
                    ,(r'Data source.*', 'Data source: <D>')
                    ,(r'(-)?At procedure.*', 'At procedure <P>')
                    ,(r'Statement\s*:.*', 'Statement: <S>')
                    ,(r'.*-port detached', '')
                ]

act = python_act('db', substitutions = substitutions)

init_script = temp_file('init_script.sql')
hang_script = temp_file('hang_script.sql')
hang_stdout = temp_file('hang_script.out')

@pytest.mark.es_eds
@pytest.mark.version('>=3.0.2')
def test_1(act: Action, init_script: Path, hang_script: Path, hang_stdout: Path, capsys):
    
    init_sql = f"""
        set term ^;
        create database '{TEST_DB_DSN}' user {act.db.user} password '{act.db.password}'
        ^
        recreate sequence g
        ^
        create or alter procedure sp_unreachable returns( unreachable_address varchar(50) ) as
        begin
            begin
                for
                    execute statement ('select mon$remote_address from mon$attachments a where a.mon$attachment_id = current_connection')
                        on external '{BOGON_IP_ADDRESS}:' || rdb$get_context('SYSTEM', 'DB_NAME')
                        as user '{act.db.user}' password '{act.db.password}' role left(replace( uuid_to_char(gen_uuid()), '-', ''), 31)
                    into unreachable_address
                do
                    suspend;
             end
        end
        ^
        create or alter procedure sp_get_data returns( unreachable_address varchar(50) ) as
        begin
            for
                execute statement ('select u.unreachable_address from sp_unreachable as u')
                    on external 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
                    as user '{act.db.user}' password '{act.db.password}' role left(replace( uuid_to_char(gen_uuid()), '-', ''), 31)
                into unreachable_address
            do
                suspend;
        end
        ^
    """
    init_script.write_text(init_sql)
    act.isql(switches=['-q'], input_file = init_script, connect_db = False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.return_code == 0 and act.stdout == '', 'Init script failed:\n\n' + act.clean_stdout

    sql_to_hang = """
        set list on;
        select current_timestamp as dts_start from rdb$database;
        select * from sp_get_data;
        select current_timestamp as dts_finish from rdb$database;
    """
    hang_script.write_text(sql_to_hang)
    pattern_for_failed_statement = re.compile('Statement failed, SQLSTATE = (08006|08003)')
    pattern_for_connection_close = re.compile('(Error (reading|writing) data (from|to) the connection)|(connection shutdown)')
    pattern_for_ignored_messages = re.compile('(-send_packet/send)|(-Killed by database administrator.)')
    killer_output_map = {}
    con_watched = None
    p_hang_sql = None

    #
    with open(hang_stdout, mode='w') as hang_out:

        # Create connection for watching mon$attachments: we are waiting appearance of 'isql' process now:
        #
        with fb_core.connect(TEST_DB_DSN, user= act.db.user, password = act.db.password) as con_monitoring:
            tpb_monitoring = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0)
            tx_monitoring = con_monitoring.transaction_manager(tpb_monitoring)
            cur_monitoring = tx_monitoring.cursor()
            sql_watched = """
                select 1 from mon$attachments
                where
                    mon$attachment_id != current_connection
                    and trim(lower(mon$remote_process)) similar to '%[\\/]isql(.exe)?'
            """
            iter = 0
            t1=py_dt.datetime.now()
            while True:
                if iter == 0:
                    #########################
                    ###   'h a n g e d'   ###
                    #########################
                    p_hang_sql = subprocess.Popen([act.vars['isql'], '-i', str(hang_script),
                                                   '-user', act.db.user,
                                                   '-password', act.db.password, TEST_DB_DSN],
                                                   stdout=hang_out, stderr=subprocess.STDOUT)

                tx_monitoring.begin()
                cur_monitoring.execute(sql_watched)
                con_watched = cur_monitoring.fetchone()
                tx_monitoring.rollback()
                if con_watched:
                    break
                else:
                    t2=py_dt.datetime.now()
                    d1=t2-t1
                    if d1.seconds*1000 + d1.microseconds//1000 >= MAX_WAIT_FOR_WORKER_START_MS:
                        break
                    else:
                        time.sleep(0.2)

                iter += 1

        #< with fb_core.connect(...) as con_monitoring

        if con_watched and p_hang_sql:

            time.sleep(2) # ::: NB ::: we have to be sure that isql will hang at obtaining data from bogon IP
            assert p_hang_sql.poll() is None, f"\nISQL process which had to hang completed too fast. Check its log:\n" + '\n'.join([ x for x in hang_stdout.read_text().splitlines() if x.strip() ]) + VPN_WARNING_MSG

            kill_script = f"""
                set list on;
                set blob all;
                connect '{TEST_DB_DSN}' user {act.db.user} password '{act.db.password}';
                select gen_id(g,1) as ITERATION_NO from rdb$database;
                commit;

                select
                     sign(a.mon$attachment_id) as hanging_attach_connection
                    ,left(a.mon$remote_protocol,3) as hanging_attach_protocol
                    ,s.mon$state as hanging_statement_state
                    ,s.mon$sql_text as hanging_statement_blob_id
                from rdb$database d
                left join mon$attachments a on a.mon$remote_process containing 'isql'
                    and a.mon$system_flag is distinct from 1
                    and a.mon$attachment_id is distinct from current_connection
                left join mon$statements s on
                    a.mon$attachment_id = s.mon$attachment_id
                    and s.mon$state = 1 -- 4.0 Classic: 'SELECT RDB$MAP_USING, RDB$MAP_PLUGIN, ... FROM RDB$AUTH_MAPPING', mon$state = 0
                ;

                set term ^;
                execute block returns(deleted_count smallint, detach_ended_time varchar(255)) as
                    declare t0 timestamp;
                    declare deleted_in_ms int;
                begin
                    t0 = 'now';
                    delete /* trace_me: iter %(iter)s */ from mon$attachments a
                    where
                        a.mon$attachment_id <> current_connection
                        and a.mon$remote_process containing 'isql';
                    deleted_count = row_count;
                    deleted_in_ms = datediff(millisecond from t0 to cast('now' as timestamp));
                    detach_ended_time = iif(deleted_in_ms < {MAX_WAIT_FOR_KILLER_FINISH_MS}, 'Acceptable.', 'TOO SLOW: ' || deleted_in_ms || ' ms -- greater than MAX_WAIT_FOR_KILLER_FINISH_MS=' || {MAX_WAIT_FOR_KILLER_FINISH_MS});
                    suspend;
                end
                ^
                commit
                ^
                --/*
                execute block as
                    declare v_dummy int;
                begin
                    begin
                        execute statement 'ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL';
                    when any do
                        begin
                        end
                    end
                end
                ^
                --*/
            """
            
            try:
                for iter in range(2):
                    ########################
                    ###    k i l l e r   ###
                    ########################
                    act.isql(switches=['-q'], input = kill_script % locals(), connect_db = False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
                    killer_output_map[ iter ] = act.clean_stdout
                    act.reset()
            finally:
                p_hang_sql.terminate()
                p_hang_sql.wait(timeout = MAX_WAIT_FOR_KILLER_FINISH_MS)
        #< if con_watched and p_hang_sql
    #< with open(hang_stdout, mode='w')

    assert con_watched and p_hang_sql, f"\nCould not find ISQL which had to hang for {MAX_WAIT_FOR_WORKER_START_MS} ms. Check its log:\n" + '\n'.join([ x for x in hang_stdout.read_text().splitlines() if x.strip() ]) + VPN_WARNING_MSG

    # 4debug only:
    #shutil.copy2(hang_script, r"C:\FBTESTING\qa\misc\core-5685-hanged.debug.sql")
    #shutil.copy2(hang_stdout, r"C:\FBTESTING\qa\misc\core-5685-hanged.debug.log")

    output = []
    for line in hang_stdout.read_text().splitlines():
        if line.strip():
            msg = ''
            if pattern_for_ignored_messages.search(line):
                continue
            elif pattern_for_failed_statement.search(line):
                msg = '<found pattern about failed statement>'
            elif pattern_for_connection_close.search(line):
                msg = '<found pattern about closed connection>'
            else:
                msg = line
    
            if msg.strip():
                output.append(f'HANGED ATTACH LOG: {msg}')

    for k in killer_output_map.keys():
        output.append(  '\n'.join( ["KILLER ATTACH LOG: " + x for x in killer_output_map[k].splitlines() if x.strip()] ) )

    #if USE_RANDOM_TEST_DB_NAME:
    #    pass
    #else:
    #    # ::: NB :::
    #    # If database will be created with FIXED name ('test_NN.fdb') then this test can not be checked in loop without
    #    # change DB state full shutdown / bring online! Because of unknown reason such DB remains opened for approx 20 seconds
    #    # and we will get 'object in use' error on 2nd iteration of such loop.
    #    # Moreover, 'gfix -shut ...' completes NOT instantly and lasts for same ~20 seconds!
    #
    #    # 'gfix -online' caused crash on 4.0.0.483 and 3.0.2.32703. This "if"-branch must be disabled if these snapshots are checked!
    #    # Ensure that database is not busy! This was detected on 5.0.4.1748, 6.0.0.1403. Linger=0 did not help.
    #    # Time of gfix -shut ... can be ~20 seconds!
    #    act.gfix( switches = ['-shutdown', 'full', '-force', '0', act.db.dsn], combine_output = True, io_enc = locale.getpreferredencoding() )
    #    assert act.return_code == 0, 'Could not change DB state to full shutdown:\n\n' + act.clean_stdout
    #    act.reset()
    #
    #    act.gfix( switches = ['-online', act.db.dsn], combine_output = True, io_enc = locale.getpreferredencoding() )
    #    assert act.return_code == 0, 'Could not bring DB online:\n\n' + act.clean_stdout
    #    act.reset()

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    STORED_PROC_NAME = "'SP_GET_DATA'" if act.is_version('<6') else '"SP_GET_DATA"'

    act.substitutions.extend( [('.*DTS_START.*', '')] )

    expected_stdout = f"""
        HANGED ATTACH LOG: Statement failed, SQLSTATE = 42000
        HANGED ATTACH LOG: Execute statement error at isc_dsql_fetch :
        HANGED ATTACH LOG: <found pattern about closed connection>
        HANGED ATTACH LOG: Statement: <S>
        HANGED ATTACH LOG: Data source: <D>
        HANGED ATTACH LOG: At procedure <P>
        HANGED ATTACH LOG: After line <N>
        HANGED ATTACH LOG: <found pattern about failed statement>
        HANGED ATTACH LOG: <found pattern about closed connection>
        HANGED ATTACH LOG: After line <N>
        HANGED ATTACH LOG: <found pattern about failed statement>
        HANGED ATTACH LOG: <found pattern about closed connection>
        HANGED ATTACH LOG: After line <N>
        HANGED ATTACH LOG: <found pattern about failed statement>
        HANGED ATTACH LOG: <found pattern about closed connection>
        HANGED ATTACH LOG: After line <N>
        KILLER ATTACH LOG: ITERATION_NO                    1
        KILLER ATTACH LOG: HANGING_ATTACH_CONNECTION       1
        KILLER ATTACH LOG: HANGING_ATTACH_PROTOCOL         TCP
        KILLER ATTACH LOG: HANGING_STATEMENT_STATE         1
        KILLER ATTACH LOG: select * from sp_get_data
        KILLER ATTACH LOG: DELETED_COUNT                   1
        KILLER ATTACH LOG: DETACH_ENDED_TIME               Acceptable.
        KILLER ATTACH LOG: ITERATION_NO                    2
        KILLER ATTACH LOG: HANGING_ATTACH_CONNECTION       <null>
        KILLER ATTACH LOG: HANGING_ATTACH_PROTOCOL         <null>
        KILLER ATTACH LOG: HANGING_STATEMENT_STATE         <null>
        KILLER ATTACH LOG: DELETED_COUNT                   0
        KILLER ATTACH LOG: DETACH_ENDED_TIME               Acceptable.
    """

    act.expected_stdout = expected_stdout
    act.stdout = '\n'.join(output)
    assert act.clean_stdout == act.clean_expected_stdout
