#coding:utf-8

"""
ID:          replication.test_journal_file_truncated_on_concurrent_cs_load.py
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8766
TITLE:       File truncation errors may happen during replication in Windows CS
DESCRIPTION:
    Fix:
    https://github.com/FirebirdSQL/firebird/commit/559b5f6c474056d6be320ad2d86fb5de473bb3a1

    Test cretes two connections (as described in the ticket).
    Second connection (in chronological order) creates table and inserts one record in it, then commits and quits.
    After this, first connection adds record to existing table (created by second connection), commits and quits.
    Then we wait until replica DB will have the same data as master for <MAX_TIME_FOR_WAIT_DATA_IN_REPLICA> seconds.
    If replace become equal to master then we check replication log for presense of 'WARNING:' or 'ERROR:' messages
    (they must NOT appear there -- see 'out_post' variable).
    Otherwise we show content of new lines in replication log that did appear (they should contain WARNING/ERROR).
NOTES:
    [03.10.2025] pzotov
    1. In case of any errors (somewhat_failed <> 0) test will re-create db_main and db_repl, and then perform all needed
       actions to resume replication (set 'replica' flag on db_repl, enabling publishing in db_main, remove all files
       from subdirectories <repl_journal> and <repl_archive> which must present in the same folder as <db_main>).
    2. CRUCIAL NOTE. File replication.conf *MUST* contain 'journal_archive_command = ...' parameter!
       Engine must NOT iself copy segments to $(archivepathname). Otherwise problem can not be reproduced.

    4. Related commits:
        4.x: https://github.com/FirebirdSQL/firebird/commit/754ec3ea915b09a112cdc6c85bc2ce051cdc6818
        5.x: https://github.com/FirebirdSQL/firebird/commit/1e8e75fbfd0ea6af174946423504733619c98d37
        6.x: https://github.com/FirebirdSQL/firebird/commit/d8102f7d7f8c97b58046d216aa944993e6d16ff9
    5. Test duration: ~7-8 seconds - if it passes. Otherwise duration is defined by MAX_TIME_* settings (see in the code).

    Confirmed bug on 6.0.0.1286-33a721b; 5.0.4.1715-4aac556; 4.0.7.3235-754ec3e
    Checked on 6.0.0.1292-83c4c5f; 5.0.4.1717-42cf2d1; 4.0.7.3236-9eefa79
"""
import os
import shutil
import re
from difflib import unified_diff
from pathlib import Path
import traceback
import time
import datetime as py_dt

import pytest
from firebird.qa import *
from firebird.driver import connect, create_database, DbWriteMode, ReplicaMode, ShutdownMode, ShutdownMethod, DatabaseError

CONCURRENT_WORKLOAD_GENERATING_ROWS = 70000

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
repl_settings = QA_GLOBALS['replication']

MAX_TIME_FOR_WAIT_SEGMENT_IN_LOG = int(repl_settings['max_time_for_wait_segment_in_log'])
MAX_TIME_FOR_WAIT_DATA_IN_REPLICA = int(repl_settings['max_time_for_wait_data_in_replica'])

MAIN_DB_ALIAS = repl_settings['MAIN_DB_8766_ALIAS']
REPL_DB_ALIAS = repl_settings['REPL_DB_8766_ALIAS']
RUN_SWEEP_AT_END = int(repl_settings['RUN_SWEEP_AT_END'])

db_main = db_factory( filename = '#' + MAIN_DB_ALIAS, do_not_create = True, do_not_drop = True)
db_repl = db_factory( filename = '#' + REPL_DB_ALIAS, do_not_create = True, do_not_drop = True)

substitutions = [
                    ('Start removing objects in:.*', 'Start removing objects'),
                    ('Finish. Total objects removed:  [1-9]\\d*', 'Finish. Total objects removed'),
                    ('.* CREATE DATABASE .*', ''),
                    ('[\t ]+', ' '),
                ]

act_db_main = python_act('db_main', substitutions=substitutions)
act_db_repl = python_act('db_repl', substitutions=substitutions)

#--------------------------------------------

def cleanup_folder(p):
    # Removed all files and subdirs in the folder <p>
    # Used for cleanup <repl_journal> and <repl_archive> when replication must be reset
    # in case when any error occurred during test execution.
    assert os.path.dirname(p) != p, f"@@@ ABEND @@@ CAN NOT operate in the file system root directory. Check your code!"

    for root, dirs, files in os.walk(p):
        for f in files:
            # ::: NB ::: 22.12.2023.
            # We have to expect that attempt to deletion of GUID and maybe some other files can FAIL with
            # PermissionError: [WinError 32] The process cannot access the file because it is being used by another process:
            # '<path/to/{GUID}'
            try:
                #os.unlink(os.path.join(root, f))
                Path(root +'/' + f).unlink(missing_ok = True)
            except PermissionError as x:
                pass

        for d in dirs:
            shutil.rmtree(os.path.join(root, d), ignore_errors = True)

    return os.listdir(p)

#--------------------------------------------

def reset_replication(act_db_main, act_db_repl, db_main_file, db_repl_file):
    out_reset = ''
    failed_shutdown_db_map = {} # K = 'db_main', 'db_repl'; V = error that occurred when we attempted to change DB state to full shutdown (if it occurred)

    with act_db_main.connect_server() as srv:

        # !! IT IS ASSUMED THAT REPLICATION FOLDERS ARE IN THE SAME DIR AS <DB_MAIN> !!
        # DO NOT use 'a.db.db_path' for ALIASED database!
        # It will return '.' rather than full path+filename.

        repl_root_path = Path(db_main_file).parent
        repl_jrn_sub_dir = Path(db_main_file).with_suffix('.journal')
        repl_arc_sub_dir = Path(db_main_file).with_suffix('.archive')

        for f in (db_main_file, db_repl_file):
            # Method db.drop() changes LINGER to 0, issues 'delete from mon$att' with suppressing exceptions
            # and calls 'db.drop_database()' (also with suppressing exceptions).
            # We change DB state to FULL SHUTDOWN instead of call action.db.drop() because
            # this is more reliable (it kills all attachments in all known cases and does not use mon$ table)
            #
            try:
                srv.database.shutdown(database = f, mode = ShutdownMode.FULL, method = ShutdownMethod.FORCED, timeout = 0)

                # REMOVE db file from disk: we can safely assume that this can be done because DB in full shutdown state.
                ###########################
                os.unlink(f)
            except DatabaseError as e:
                failed_shutdown_db_map[ f ] = e.__str__()


        # Clean folders repl_journal and repl_archive: remove all files from there.
        # NOTE: test must NOT raise unrecoverable error if some of files in these folders can not be deleted.
        # Rather, this must be displayed as diff and test must be considered as just failed.
        for p in (repl_jrn_sub_dir,repl_arc_sub_dir):
            remained_files = cleanup_folder(p)

            if remained_files:
                out_reset += '\n'.join( (f"Directory '{str(p)}' remains non-empty. Could not delete file(s):", '\n'.join(remained_files)) )

    # xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    # xxx  r e c r e a t e     d b _ m a i n     a n d     d b _ r e p l  xxx
    # xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    for a in (act_db_main,act_db_repl):
        d = a.db.db_path
        failed_shutdown_msg = failed_shutdown_db_map.get( str(d), '' )
        if failed_shutdown_msg:
            # we could NOT change state of this database to full shutdown --> we must NOT recreate it.
            # Accumulate error messages in OUT arg (for displaying as diff):
            #
            out_reset += '\n'.join( failed_shutdown_msg )
        else:
            try:
                dbx = create_database(str(d), user = a.db.user)
                dbx.close()
                with a.connect_server() as srv:
                    srv.database.set_write_mode(database = d, mode = DbWriteMode.ASYNC)
                    srv.database.set_sweep_interval(database = d, interval = 0)
                    if a == act_db_repl:
                        srv.database.set_replica_mode(database = d, mode = ReplicaMode.READ_ONLY)
                    else:
                        with a.db.connect() as con:
                            con.execute_immediate('alter database enable publication')
                            con.execute_immediate('alter database include all to publication')
                            con.commit()
            except DatabaseError as e:
                out_reset += e.__str__()
        
    # Must remain EMPTY:
    ####################
    return out_reset

#--------------------------------------------

def watch_replica( a: Action, max_allowed_time_for_wait, ddl_ready_query = '', isql_check_script = '', replica_expected_out = ''):

    result = ''
    retcode = 1;
    ready_to_check = False
    if ddl_ready_query:
        with a.db.connect(no_db_triggers = True) as con:
            with con.cursor() as cur:
                for i in range(0,max_allowed_time_for_wait):
                    cur.execute(ddl_ready_query)
                    count_actual = cur.fetchone()
                    if count_actual:
                        ready_to_check = True
                        break
                    else:
                        con.rollback()
                        time.sleep(1)
    else:
        ready_to_check = True

    msg = f'UNEXPECTED. Check query did not return any rows for {max_allowed_time_for_wait} seconds.'

    if not ready_to_check:
        result = '\n'.join((msg, 'ddl_ready_query:', ddl_ready_query))
    else:
        final_check_pass = False
        if isql_check_script:
            retcode = 0
            for i in range(max_allowed_time_for_wait):
                a.reset()
                a.expected_stdout = replica_expected_out
                a.isql(switches=['-q', '-nod'], input = isql_check_script, combine_output = True)

                if a.return_code:
                    # "Token unknown", "Name longer than database column size" etc: we have to
                    # immediately break from this loop because isql_check_script is incorrect!
                    break
                
                if a.clean_stdout == a.clean_expected_stdout:
                    final_check_pass = True
                    break
                if i < max_allowed_time_for_wait-1:
                    time.sleep(1)

            if not final_check_pass:
                
                result = '\n'.join( 
                                    (  msg
                                      ,'Final check query:'
                                      ,isql_check_script
                                      ,'Expected output:'
                                      ,a.clean_expected_stdout
                                      ,'Actual output:'
                                      ,a.clean_stdout
                                      ,f'ISQL return_code={a.return_code}'
                                      ,f'Waited for {i} seconds'
                                    )
                                  )
            a.reset()
        else:
            final_check_pass = True

    return result # empty string --> OK

#--------------------------------------------

def drop_db_objects(act_db_main: Action,  act_db_repl: Action, capsys):

    # return initial state of master DB:
    # remove all DB objects (tables, views, ...):
    #
    db_main_meta, db_repl_meta = '', ''
    result = ''
    for a in (act_db_main,act_db_repl):
        if a == act_db_main:
            sql_clean = (a.files_dir / 'drop-all-db-objects.sql').read_text()
            a.expected_stdout = """
                Start removing objects
                Finish. Total objects removed
            """
            a.isql(switches=['-q', '-nod'], input = sql_clean, combine_output = True)

            if a.clean_stdout == a.clean_expected_stdout:
                a.reset()
            else:
                result = a.clean_stdout
                # print(a.clean_stdout)
                a.reset()
                break

            # NB: one need to remember that rdb$system_flag can be NOT ONLY 1 for system used objects!
            # For example, it has value =3 for triggers that are created to provide CHECK-constraints,
            # Custom DB objects always have rdb$system_flag = 0 (or null for some very old databases).
            # We can be sure that there are no custom DB objects if following query result is NON empty:
            #
            ddl_ready_query = """
                select 1
                from rdb$database
                where NOT exists (
                    select custom_db_object_flag
                    from (
                        select rt.rdb$system_flag as custom_db_object_flag from rdb$triggers rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$relations rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$functions rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$procedures rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$exceptions rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$fields rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$collations rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$generators rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$roles rt
                        UNION ALL
                        select rt.rdb$system_flag from rdb$auth_mapping rt
                        UNION ALL
                        select 1 from sec$users s
                        where upper(s.sec$user_name) <> 'SYSDBA'
                    ) t
                    where coalesce(t.custom_db_object_flag,0) = 0
                )
            """

            ##############################################################################
            ###  W A I T   U N T I L    R E P L I C A    B E C O M E S   A C T U A L   ###
            ##############################################################################
            result = watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query)

            # Must be EMPTY:
            db_main_meta = a.extract_meta(charset = 'utf8', io_enc = 'utf8')
        else:
            db_repl_meta = a.extract_meta(charset = 'utf8', io_enc = 'utf8')

        if RUN_SWEEP_AT_END:
            # Following sweep was mandatory during 2021...2022. Problem was fixed:
            # * for FB 4.x: 26-jan-2023, commit 2ed48a62c60c029cd8cb2b0c914f23e1cb56580a
            # * for FB 5.x: 20-apr-2023, commit 5af209a952bd2ec3723d2c788f2defa6b740ff69
            # (log message: 'Avoid random generation of field IDs, respect the user-specified order instead').
            # Until this problem was solved, subsequent runs of this test caused to fail with:
            # 'ERROR: Record format with length NN is not found for table TEST'
            #
            a.gfix(switches=['-sweep', a.db.dsn])

    if result == '':
        # Final point: metadata must become equal:
        #
        diff_meta = ''.join(unified_diff( \
                             [x for x in db_main_meta.splitlines() if 'CREATE DATABASE' not in x],
                             [x for x in db_repl_meta.splitlines() if 'CREATE DATABASE' not in x])
                           )
        # Must be EMPTY:
        return diff_meta
    else:
        return result

#--------------------------------------------

@pytest.mark.replication
@pytest.mark.platform('Windows')
@pytest.mark.version('>=4.0.7')
def test_1(act_db_main: Action,  act_db_repl: Action, capsys):

    if act_db_main.vars['server-arch'] == 'SuperServer':
        pytest.skip("n/a for SuperServer")

    # Map for storing mnemonas and details for every FAILED step:
    run_errors_map = {}

    db_info = {}
    out_main = out_post = out_drop = out_reset = ''

    # Obtain full path + filename for DB_MAIN and DB_REPL aliases.
    # NOTE: we must NOT use 'a.db.db_path' for ALIASED databases!
    # It will return '.' rather than full path+filename.
    # Use only con.info.name for that!
    #
    for a in (act_db_main, act_db_repl):
        with a.db.connect(no_db_triggers = True) as con:
            db_info[a,  'db_full_path'] = con.info.name
            db_info[a,  'db_fw_initial'] = con.info.write_mode

    with act_db_repl.connect_server() as srv:
        srv.database.set_write_mode(database = act_db_repl.db.db_path, mode = DbWriteMode.SYNC)

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    diff_data = []
    replication_log = act_db_main.home_dir / 'replication.log'
    replold_lines = []
    with open(replication_log, 'r') as f:
        replold_lines = f.readlines()
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    sql_init = f"""
        set bail on;
        create or alter procedure sp_gen_uuid as begin end;
        set term ^;
        execute block as
        begin
           execute statement 'drop domain dm_txt';
           when any do
           begin
               -- nop --
           end
        end ^
        set term ^;
        commit;

        create domain dm_txt varchar(1000) not null;
        create or alter procedure sp_gen_uuid as begin end;
        recreate table test (
            id int generated by default as identity primary key using descending index test_pk_desc
            ,who varchar(10)
            ,txt dm_txt unique using index test_txt_unq
        );
        commit;

        set term ^;
        alter procedure sp_gen_uuid(n_rows int) returns(o_txt dm_txt) as
            declare v_fld_len smallint;
        begin
            select
                 f.rdb$character_length as field_char_len
            from rdb$relation_fields rf
            join rdb$fields f on rf.rdb$field_source = f.rdb$field_name
            where rf.rdb$relation_name = upper('TEST') and rf.rdb$field_name = upper('txt')
            into v_fld_len;
            while (n_rows > 0) do
            begin
                o_txt = lpad('', v_fld_len, uuid_to_char(gen_uuid()));
                n_rows = n_rows - 1;
                suspend;
            end
        end
        ^
        set term ;^
        commit;

        recreate table t_completed(id int primary key);
        commit;
    """
    act_db_main.isql(switches=['-q'], input = sql_init, combine_output = True)
    run_errors_map['out_prep_ddl'] = act_db_main.clean_stdout
    act_db_main.reset()

    if max(v.strip() for v in run_errors_map.values()):
        # Some problem raised during init_sql execution
        pass
    else:
        # Query to be used for check that all DB objects present in replica (after last DML statement completed on master DB):
        ddl_ready_query = "select 1 from rdb$relations where rdb$relation_name = upper('t_completed')"
        ######################################################
        ###  WAIT UNTIL REPLICA GETS INITIAL DDL AND DATA  ###
        ######################################################
        watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query)

        # Must be EMPTY:
        run_errors_map['out_repl_ddl'] = capsys.readouterr().out

        # Get content of firebird.log BEFORE test
        log_before = act_db_main.get_firebird_log()

        try:
            with act_db_main.db.connect() as con1:
                con1.execute_immediate("insert into test(who, txt) values('att_1', 'init')")
                with act_db_main.db.connect() as con2:
                    con2.execute_immediate( f"insert into test(who, txt) select 'att-2', p.o_txt from sp_gen_uuid( {CONCURRENT_WORKLOAD_GENERATING_ROWS} ) as p")
                    con2.execute_immediate("insert into t_completed(id) values(1)")
                    con2.commit()
                con1.commit()
        except Exception as e:
            for x in traceback.format_exc().split('\n'):
                print('  ',x)
        run_errors_map['out_segm_handling']  = capsys.readouterr().out

        log_after = act_db_main.get_firebird_log()

        p_fb_log = re.compile('error |journal|truncate|((reading|writing) data (from|to))', re.IGNORECASE)
        fb_log_diff = '\n'.join( [line.rstrip() for line in unified_diff(log_before, log_after) if line.startswith('+') and line.split('+') and p_fb_log.search(line) ] )
        run_errors_map['fb_log_diff'] = fb_log_diff

        with open(replication_log, 'r') as f:
            diff_data = unified_diff(
                replold_lines,
                f.readlines()
              )

        # Post-check: verify that diff between old and current replication.log does NOT contain 'WARNING:' or 'ERROR:' lines:
        repl_log_diff = '\n'.join( [ line.rstrip() for line in diff_data if 'WARNING:' in line or 'ERROR:' in line ] )
        run_errors_map['repl_log_diff'] = repl_log_diff
        
    if max(v.strip() for v in run_errors_map.values()):
        # Some problem was in the code executed by con1 and/or con2
        pass
    else:
        ##############################################################################
        ###  W A I T   U N T I L    R E P L I C A    B E C O M E S   A C T U A L   ###
        ##############################################################################
        # ( a: Action, max_allowed_time_for_wait, ddl_ready_query = '', isql_check_script = '', replica_expected_out = ''):

        isql_expected_out = f"""
            1
        """
        isql_check_script  = f"set heading off;select id from t_completed;"
        watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, '', isql_check_script, isql_expected_out )

        # Must be EMPTY:
        run_errors_map['out_repl_data'] = capsys.readouterr().out

    # Must be EMPTY:
    #for k,v in run_errors_map.items():
    #    if v.strip():
    #        print(k,':::',v)

    run_errors_map['final_reset'] = reset_replication(act_db_main, act_db_repl, db_info[act_db_main,'db_full_path'], db_info[act_db_repl,'db_full_path'])
    # out_drop = drop_db_objects(act_db_main, act_db_repl, capsys)

    if max(v.strip() for v in run_errors_map.values()):
        print(f'Problem(s) detected, check run_errors_map:')
        for k,v in run_errors_map.items():
            if v.strip():
                print(k,':')
                print(v.strip())
                print('-' * 40)
    
    assert '' == capsys.readouterr().out
