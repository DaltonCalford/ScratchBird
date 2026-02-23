#coding:utf-8

"""
ID:          replication.oltp_emul_ddl
TITLE:       Applying full DDL from OLTP-EMUL test on master with further check replica
DESCRIPTION:
    We create table and add data in it according to ticket info.
    The last DDL that we do is creating table with name 't_completed'. It serves as 'flag' to be checked that all DDL actions
    on master finished.

    After this we wait until replica becomes actual to master, and this delay will last no more then threshold that
    is defined by MAX_TIME_FOR_WAIT_DATA_IN_REPLICA variable (measured in seconds), see QA_ROOT/files/test_config.ini

    When table 't_completed' will appear in replica, we run query to the table 'test' in order to check that it contains
    only data that were allowed on master.

    Further, we invoke ISQL with executing auxiliary script for drop all DB objects on master (with '-nod' command switch).
    After all objects will be dropped, we have to wait again until replica becomes actual with master.
    Check that both DB have no custom objects is performed (see UNION-ed query to rdb$ tables + filtering on rdb$system_flag).

    Finally, we extract metadata for master and replica and make comparison.
    The only difference in metadata must be 'CREATE DATABASE' statement with different DB names - we suppress it,
    thus metadata difference must not be issued.
FBTEST:      tests.functional.replication.oltp_emul_ddl
NOTES:
    [22.04.2023] pzotov
    Test was fully re-implemented. We have to query replica DATABASE for presense of data that we know there must appear.
    We have to avoid query of replication log - not only verbose can be disabled, but also because code is too complex.

    We use 'assert' only at the final point of test, with printing detalization about encountered problem(s).
    During all previous steps, we only store unexpected output to variables, e.g.: out_main = capsys.readouterr().out etc.
    
    [07.09.2023] pzotov
    Added 'DEBUG_MODE' variable for quick switch to debug branches if something goes wrong.
    Added code to explicitly assign 'fb_port' to default value (3050), with check that port is listening (see 'import socket')
    Replaced hardcoded 'SYSDBA' and 'masterkey' with act.db.user and act.db.password
    (see call of generate_sync_settings_sql() and its code)
    
    Checked on Linux 5.0.0.1190 CS with default firebird.conf and firebird-driver.conf without port specifying
    (see letters from dimitr, 06.09.2023)

    [22.12.2023] pzotov
    Refactored: make test more robust when it can not remove some files from <repl_journal> and <repl_archive> folders.
    This can occurs because engine opens <repl_archive>/<DB_GUID> file every 10 seconds and check whether new segments must be applied.
    Because of this, attempt to drop this file exactly at that moment causes on Windows "PermissionError: [WinError 32]".
    This error must NOT propagate and interrupt entire test. Rather, we must only to log name of file that can not be dropped.

    [23.11.2023] pzotov
    Make final SWEEP optional, depending on setting RUN_SWEEP_AT_END - see $QA_ROOT/files/test_config.ini.

    Checked on Windows, 6.0.0.193, 5.0.0.1304, 4.0.5.3042 (SS/CS for all).
"""

import os
import locale
import shutil
import zipfile
import socket
from difflib import unified_diff
from pathlib import Path
import time

import pytest
from firebird.qa import *
from firebird.driver import connect, create_database, DbWriteMode, ReplicaMode, ShutdownMode, ShutdownMethod, DatabaseError

DEBUG_MODE = 0

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
repl_settings = QA_GLOBALS['replication']

MAX_TIME_FOR_WAIT_DATA_IN_REPLICA = int(repl_settings['max_time_for_wait_data_in_replica'])
MAIN_DB_ALIAS = repl_settings['main_db_alias']
REPL_DB_ALIAS = repl_settings['repl_db_alias']
RUN_SWEEP_AT_END = int(repl_settings['run_sweep_at_end'])

db_main = db_factory( filename = '#' + MAIN_DB_ALIAS, do_not_create = True, do_not_drop = True)
db_repl = db_factory( filename = '#' + REPL_DB_ALIAS, do_not_create = True, do_not_drop = True)

substitutions = [('Start removing objects in:.*', 'Start removing objects'),
                 ('Finish. Total objects removed:  [1-9]\\d*', 'Finish. Total objects removed'),
                 ('.* CREATE DATABASE .*', ''),
                ]

act_db_main = python_act('db_main', substitutions=substitutions)
act_db_repl = python_act('db_repl', substitutions=substitutions)

tmp_oltp_build_sql = temp_file('tmp_oltp_emul_ddl.sql')
tmp_oltp_build_log = temp_file('tmp_oltp_emul_ddl.log')

#--------------------------------------------

def cleanup_folder(p):
    # Removed all files and subdirs in the folder <p>
    # Used for cleanup <repl_journal> and <repl_archive> when replication must be reset
    # in case when any error occurred during test execution.
    assert os.path.dirname(p) != p, f"@@@ ABEND @@@ CAN NOT operate in the file system root directory. Check your code!"

    for root, dirs, files in os.walk(p):
        for f in files:
            # ::: NB ::: 22.12.2023.
            # We have to expect that attempt to delete of GUID and (maybe) archived segments can FAIL with
            # PermissionError: [WinError 32] The process cannot ... used by another process: /path/to/{GUID}
            # Also, we have to skip exception if file (segment) was just deleted by engine
            try:
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
        repl_jrn_sub_dir = repl_settings['journal_sub_dir']
        repl_arc_sub_dir = repl_settings['archive_sub_dir']

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
            
            remained_files = cleanup_folder(repl_root_path/p)

            if remained_files:
                out_reset += '\n'.join( (f"Directory '{str(repl_root_path/p)}' remains non-empty. Could not delete file(s):", '\n'.join(remained_files)) )

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

    if not ready_to_check:
        print( f'UNEXPECTED. Query to check DDL completion did not return any rows for {max_allowed_time_for_wait} seconds.' )
        print('Initial check query:')
        print(ddl_ready_query)
        return
    
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
            print(f'UNEXPECTED. Final check query did not return expected dataset for {max_allowed_time_for_wait} seconds.')
            print('Final check query:')
            print(isql_check_script)
            print('Expected output:')
            print(a.clean_expected_stdout)
            print('Actual output:')
            print(a.clean_stdout)
            print(f'ISQL return_code={a.return_code}')
            print(f'Waited for {i} seconds')

        a.reset()

    else:
        final_check_pass = True

    return

#--------------------------------------------

def drop_db_objects(act_db_main: Action,  act_db_repl: Action, capsys):

    # return initial state of master DB:
    # remove all DB objects (tables, views, ...):
    #
    db_main_meta, db_repl_meta = '', ''
    drop_failed_txt = ''
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
                drop_failed_txt = '\n'.join( (f'Problem with DROP all objects in {a.db.db_path}:', a.clean_stdout) )
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
            watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query)

            # Must be EMPTY:
            print(capsys.readouterr().out)

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

    if drop_failed_txt:
        # No sens to compare metadata if we could not drop all objects in any of databases.
        print(drop_failed_txt)
        pass
    else:
        # Final point: metadata must become equal:
        #
        diff_meta = ''.join(unified_diff( \
                             [x for x in db_main_meta.splitlines() if 'CREATE DATABASE' not in x],
                             [x for x in db_repl_meta.splitlines() if 'CREATE DATABASE' not in x])
                           )
        # Must be EMPTY:
        if diff_meta:
            print('Metadata differs after DROP all objects')
        print(diff_meta)

#--------------------------------------------

def generate_sync_settings_sql(db_main_file_name, dba_usr, dba_psw, fb_port = 3050):

    def generate_inject_setting_sql(working_mode, mcode, new_value, allow_insert_if_eof = 0):
        sql_inject_setting = ''
        if allow_insert_if_eof == 0:
            sql_inject_setting = '''
                update settings set svalue = %(new_value)s
                where working_mode = upper('%(working_mode)s') and mcode = upper('%(mcode)s');
                if (row_count = 0) then
                    exception ex_record_not_found using('SETTINGS', q'{working_mode = upper('%(working_mode)s') and mcode = upper('%(mcode)s')}');
            ''' % locals()
        else:
            sql_inject_setting = '''
                update or insert into settings(working_mode, mcode, svalue)
                values( upper('%(working_mode)s'), upper('%(mcode)s'), %(new_value)s )
                matching (working_mode, mcode);
            ''' % locals()

        return sql_inject_setting

    sql_adjust_settings_table = '''
        set list on;
        select 'Adjust settings: start at ' || cast('now' as timestamp) as msg from rdb$database;
        set term ^;
        execute block as
        begin
    '''

    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'init', 'working_mode', "upper('small_03')" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'enable_mon_query', "'0'" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'unit_selection_method', "'random'" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'build_with_split_heavy_tabs', "'1'" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'build_with_qd_compound_ordr', "lower('most_selective_first')" ) ) )

    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'build_with_separ_qdistr_idx', "'0'" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'used_in_replication', "'1'" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'separate_workers', "'1'" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'workers_count', "'100'" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'update_conflict_percent', "'0'" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'init', 'connect_str', "'connect ''localhost:%(db_main_file_name)s'' user ''%(dba_usr)s'' password ''%(dba_psw)s'';'" % locals(), 1 ) ) )

    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'mon_unit_list', "'//'" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'halt_test_on_errors', "'/PK/CK/'" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'qmism_verify_bitset', "'1'" ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'recalc_idx_min_interval', "'9999999'" ) ) )

    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'warm_time', "'0'", 1 ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'test_intervals', "'10'", 1 ) ) )

    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'init', 'tmp_worker_role_name', "upper('tmp$oemul$worker')", 1 ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'init', 'tmp_worker_user_prefix', "upper('tmp$oemul$user_')", 1 ) ) )


    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'common', 'use_es', "'2'", 1 ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'init', 'host', "'localhost'", 1 ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'init', 'port', "'%(fb_port)s'" % locals(), 1 ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'init', 'usr', "'%(dba_usr)s'" % locals(), 1 ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'init', 'pwd', "'%(dba_psw)s'" % locals(), 1 ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'init', 'tmp_worker_user_pswd', "'0Ltp-Emu1'", 1 ) ) )

    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'init', 'conn_pool_support', "'1'", 1 ) ) )
    sql_adjust_settings_table = ''.join( (sql_adjust_settings_table, generate_inject_setting_sql( 'init', 'resetting_support', "'1'", 1 ) ) )

    sql_adjust_settings_table +=     '''
        end ^
        set term ;^
        commit;
        select 'Adjust settings: finish at ' || cast('now' as timestamp) as msg from rdb$database;
        set list off;
    '''

    return sql_adjust_settings_table

#--------------------------------------------
def get_replication_log(a: Action):

    replication_log = a.home_dir / 'replication.log'
    rlog_lines = []
    with open(replication_log, 'r') as f:
        rlog_lines = f.readlines()

    return rlog_lines

#--------------------------------------------

@pytest.mark.replication
@pytest.mark.version('>=4.0.1')
def test_1(act_db_main: Action,  act_db_repl: Action, tmp_oltp_build_sql: Path, tmp_oltp_build_log: Path, capsys):

    if act_db_main.is_version('>=6.0'):
        pytest.skip('Since #8473 empty columns not allowed')

    tmp_oltp_sql_files = []
    
    out_prep, out_init, out_main, out_drop = '', '', '', ''
    # Obtain full path + filename for DB_MAIN and DB_REPL aliases.
    # NOTE: we must NOT use 'a.db.db_path' for ALIASED databases!
    # It will return '.' rather than full path+filename.
    # Use only con.info.name for that!
    #
    db_info = {}
    for a in (act_db_main, act_db_repl):
        with a.db.connect(no_db_triggers = True) as con:
            #if a == act_db_main and a.vars['server-arch'] == 'Classic' and os.name != 'nt':
            #    pytest.skip("Waiting for FIX: 'Engine is shutdown' in replication log for CS. Linux only.")
            db_info[a,  'db_full_path'] = con.info.name


    repl_log_old = get_replication_log(act_db_main)
    repl_log_new = repl_log_old.copy()

    # NB, 06-sep-2023: we have to explicitly assign default value for listening port because OLTP-EMUL requires it always.
    # If $QA_HOME/firebird-driver.conf does not contain line with this parameter then act_db_main.port will be None.
    # In that case execution of some scripts will fail because of conversion error from string 'None' to int variable.
    # This problem was detected for script 'oltp-emul-09_adjust_eds_calls.sql':
    #     declare v_port int;
    #     ...
    #     select ... max( iif( upper(s.mcode) = upper('port'), s.svalue , null ) )
    #     from settings s 
    #     where ...
    #     into v_port ... ; <<< THIS WILL FAIL with 'conversion error from "None"'.

    fb_port = 3050
    if act_db_main.port:
        fb_port = int(act_db_main.port) if act_db_main.port.isdigit() else fb_port

    # Additional check: fb_port must be listening now:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect(('localhost', fb_port))

    # Must be EMPTY:
    out_prep = capsys.readouterr().out
    if out_prep:
        # Some problem raised during establishing connections to master/replica DB
        pass
    else:
        oltp_files = Path(act_db_main.files_dir / 'oltp-emul-ddl.zip')
        with zipfile.ZipFile(oltp_files, 'r') as zip_ref:
            zip_ref.extractall(tmp_oltp_build_sql.parents[0])
            tmp_oltp_sql_files = [ Path(tmp_oltp_build_sql.parents[0]/x) for x in zip_ref.namelist() ]
        sql_build_init = '\n'.join([ 'in ' + str(x) + ';' for x in tmp_oltp_sql_files[:5]])

        #--------------------------------------------------------------------------------
        with open(tmp_oltp_build_sql, 'w') as f:
            f.write( sql_build_init )
            f.write( generate_sync_settings_sql(db_info[act_db_main,  'db_full_path'], act_db_main.db.user, act_db_main.db.password, fb_port) )
        #--------------------------------------------------------------------------------

        oltp_post_files = tmp_oltp_sql_files[5:10] # 'oltp-emul-06_split_heavy_tabs.sql' ... 'oltp-emul-10_adjust_eds_perf.sql'
        sql_build_post = ''
        tmp_post_handling_lst = []
        for f in oltp_post_files:
            tmp_post_handling_sql = f.with_suffix('.tmp')
            tmp_post_handling_sql.unlink(missing_ok = True)

            tmp_post_handling_lst.append(tmp_post_handling_sql)
            sql_build_post += f"""
                out {tmp_post_handling_sql};
                in {f};
                out;
                in {tmp_post_handling_sql};
            """
            if  'adjust_eds_calls' in str(f):
                # We have to make COMMIT here, otherwise get:
                # Statement failed, SQLSTATE = 2F000
                # Error while parsing procedure SP_PERF_EDS_LOGGING's BLR
                # -attempted update of read-only column <unknown>
                # After line ... in file ... oltp-emul-10_adjust_eds_perf.sql
                #
                sql_build_post += "\ncommit;\n"

        with open(tmp_oltp_build_sql, 'a') as f:
            f.write( sql_build_post )

        oltp_final_files = tmp_oltp_sql_files[10:]  # 'oltp-emul-11_ref_data_filling.sql', 'oltp-emul-12_activate_db_triggers.sql'
        sql_build_final = '\n'.join( [ f'in {x};' for x in oltp_final_files] )

        sql_completed = """
            recreate table t_completed(id int primary key);
            commit;
        """
        with open(tmp_oltp_build_sql, 'a') as f:
            f.write( sql_build_final )
            f.write( sql_completed )

        tmp_oltp_sql_files += tmp_post_handling_lst

        act_db_main.isql(switches=['-q', '-nod'], input_file = str(tmp_oltp_build_sql), io_enc = locale.getpreferredencoding(), combine_output = True)
        if act_db_main.return_code:
            out_init= '\n'.join( ('act_db_main.return_code = %d' % act_db_main.return_code, act_db_main.clean_stdout ) )

        act_db_main.reset()

        if DEBUG_MODE:
            pass
        else:
            for p in tmp_oltp_sql_files:
                p.unlink(missing_ok = True)

        repl_log_new = get_replication_log(act_db_main)

    if out_init:
        # Some problem raised during execution of initial SQL
        pass
    else:


        # Query to be used for check that all DB objects present in replica (after last DML statement completed on master DB):
        ddl_ready_query = "select 1 from rdb$relations where rdb$relation_name = upper('t_completed')"

        # Query to be used that replica DB contains all expected data (after last DML statement completed on master DB):
        isql_check_script = """
                set list on;
                set count on;
                select
                     rdb$get_context('SYSTEM','REPLICA_MODE') replica_mode
                    ,s.task
                from semaphores s
                where s.id = -1;
        """

        isql_expected_out = f"""
            REPLICA_MODE                    READ-ONLY
            TASK                            all_build_ok
            Records affected: 1
        """

        ##############################################################################
        ###  W A I T   U N T I L    R E P L I C A    B E C O M E S   A C T U A L   ###
        ##############################################################################
        watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query, isql_check_script, isql_expected_out)
        # Must be EMPTY:
        out_main = capsys.readouterr().out
        repl_log_new = get_replication_log(act_db_main)

    if DEBUG_MODE:
        pass
    else:
        drop_db_objects(act_db_main, act_db_repl, capsys)

    # Must be EMPTY:
    out_drop = capsys.readouterr().out

    if [ x for x in (out_prep, out_init, out_main, out_drop) if x.strip() ]:
        # We have a problem either with DDL/DML or with dropping DB objects.
        # First, we have to RECREATE both master and slave databases
        # (otherwise further execution of this test or other replication-related tests most likely will fail):
        out_reset = reset_replication(act_db_main, act_db_repl, db_info[act_db_main,'db_full_path'], db_info[act_db_repl,'db_full_path'], cleanup_repl_dirs = not DEBUG_MODE)

        # Next, we display out_main, out_drop and out_reset:
        #
        print('Problem(s) detected:')
        if out_prep.strip():
            print('out_prep:')
            print(out_prep)
        if out_init.strip():
            print('out_init:')
            print(out_init)
        if out_main.strip():
            print('out_main:')
            print(out_main)
        if out_drop.strip():
            print('out_drop:')
            print(out_drop)
        if out_reset.strip():
            print('out_reset:')
            print(out_reset)

        # Finally, we have to show content of replication.log afte this test started:
        print('Lines that did appear in replication.log during test run:')
        for line in unified_diff(repl_log_old, repl_log_new):
            if line.startswith('+') and line[2:].strip():
                print(line.strip())

    assert '' == capsys.readouterr().out
