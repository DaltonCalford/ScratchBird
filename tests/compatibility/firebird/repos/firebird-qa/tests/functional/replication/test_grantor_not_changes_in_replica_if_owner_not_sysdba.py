#coding:utf-8

"""
ID:          replication.test_grantor_not_changes_in_replica_if_owner_not_sysdba
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8058
TITLE:       DDL-Changes in replication does not set the correct grantor
DESCRIPTION:
    Test creates user with admin rights (see 'db_main_owner') and calls 'reset_replication' function in order
    to re-create db_main database with new OWNER  = <db_main_owner>, i.e. it must differ from SYSDBA.
    Then it does actions described in the ticket.
    Final REVOKE command being issued against db_main must apply also in db_repl, w/o error.
    Test verifies that by checking result of query:
        select 1 as db_repl_privilege_unexp_remains from rdb$database
        where exists (
            select 1 from rdb$user_privileges p where p.rdb$relation_name = upper('test') and p.rdb$privilege = upper('D')
        );
    Outcome of this query on REPLICA database must become empty for no more than MAX_TIME_FOR_WAIT_DATA_IN_REPLICA seconds.
    Otherwise test is considered as failed. 

NOTES:
    [15.12.2024] pzotov
    Before fix, following messages did appear in replication log:
        ERROR: unsuccessful metadata update
        REVOKE failed
        <db_main_owner> is not grantor of DELETE on TEST to <db_main_owner>.

    We have to restore owner = SYSDBA for db_main, so we call 'reset_replication' function second time at final point.
    Test execution time is about 7...8 seconds (for snapshots that have fix).

    Confirmed bug on 6.0.0.299, 5.0.1.1371, 4.0.5.3082
    Checked on 6.0.0.552, 5.0.2.1569, 4.0.6.3170.
"""
import os
import shutil
import re
import locale
from difflib import unified_diff
from pathlib import Path
import datetime
import time

import pytest
from firebird.qa import *
from firebird.driver import * 

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
repl_settings = QA_GLOBALS['replication']

MAX_TIME_FOR_WAIT_SEGMENT_IN_LOG = int(repl_settings['max_time_for_wait_segment_in_log'])
MAX_TIME_FOR_WAIT_DATA_IN_REPLICA = int(repl_settings['max_time_for_wait_data_in_replica'])

MAIN_DB_ALIAS = repl_settings['main_db_alias']
REPL_DB_ALIAS = repl_settings['repl_db_alias']
RUN_SWEEP_AT_END = int(repl_settings['run_sweep_at_end'])


db_main = db_factory( filename = '#' + MAIN_DB_ALIAS, do_not_create = True, do_not_drop = True)
db_repl = db_factory( filename = '#' + REPL_DB_ALIAS, do_not_create = True, do_not_drop = True)

db_main_owner = user_factory('db_main', name = 'tmp_gh_8058', password = '456', admin = True)

substitutions = [('Start removing objects in:.*', 'Start removing objects'),
                 ('Finish. Total objects removed:  [1-9]\\d*', 'Finish. Total objects removed'),
                 ('.* CREATE DATABASE .*', ''),
                 ('[\t ]+', ' '),
                 ('FOUND message about replicated segment N .*', 'FOUND message about replicated segment')]

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

# ::: NB :::
# THIS FUNCTION HAS ADDITIONAL INPUT PARAMETER: 'db_main_owner' 
#
def reset_replication(act_db_main: Action, act_db_repl: Action, db_main_file, db_repl_file, db_main_owner: User = None):
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
                if db_main_owner and a == act_db_main:
                    dbx = create_database( str(d), user = db_main_owner.name, password = db_main_owner.password )
                else:
                    dbx = create_database( str(d), user = a.db.user, password = a.db.password )
                dbx.close()
                with a.connect_server() as srv:
                    srv.database.set_write_mode(database = d, mode = DbWriteMode.ASYNC)
                    srv.database.set_sweep_interval(database = d, interval = 0)
                    if a == act_db_repl:
                        srv.database.set_replica_mode(database = d, mode = ReplicaMode.READ_ONLY)
                    else:
                        with a.db.connect() as con:
                            con.execute_immediate(f'alter database enable publication')
                            con.execute_immediate('alter database include all to publication')
                            con.commit()
            except DatabaseError as e:
                out_reset += e.__str__()
        
    # Must remain EMPTY:
    ####################
    return out_reset

#--------------------------------------------

def watch_repl_log_pattern( act_db_main: Action, pattern_to_check, replold_lines, max_allowed_time_for_wait, consider_found_as_unexpected = False):

    replication_log = act_db_main.home_dir / 'replication.log'

    result = ''
    found_required_message = False
    found_required_line = ''
    t0 = time.time()
    for i in range(0,max_allowed_time_for_wait):

        time.sleep(1)

        with open(replication_log, 'r') as f:
            diff_data = unified_diff(
                replold_lines,
                f.readlines()
              )

        for k,d in enumerate(diff_data):
            if pattern_to_check.search(d):
                found_required_message = True
                break

        if found_required_message:
            break
    t1 = time.time()
    if not consider_found_as_unexpected and not found_required_message or consider_found_as_unexpected and found_required_message:
        # ACHTUNG! This looks weird but we have to either re-read replication log now or wait at least <JOURNAL_ARCHIVE_TIMEOUT> seconds
        # if we want to see FULL (actual) content of this log! Otherwise last part of log will be missed. I have no explanations for that :(
        repllog_diff = ''
        with open(replication_log, 'r') as f:
            diff_data = unified_diff(
                replold_lines,
                f.readlines()
              )
            repllog_diff = '\n'.join( ( ('%4d ' %i) + r.rstrip() for i,r in enumerate(diff_data) ) )

        if consider_found_as_unexpected:
            unexp_msg = f"UNEXPECTED outcome: pattern '{pattern_to_check.pattern}' must not occur in log but was ENCOUNTERED there for {int(t1-t0)} seconds."
        else:
            unexp_msg = f"MISSED outcome: pattern '{pattern_to_check.pattern}' was NOT FOUND for {int(t1-t0)} seconds."

        result = '\n'.join( (unexp_msg, 'replication log diff:', repllog_diff) )

    return result

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
        print( f'UNEXPECTED. Query to verify DDL completion did not return any rows for {max_allowed_time_for_wait} seconds.' )
        print('Query:')
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
                print(a.clean_expected_stdout)
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

    # Final point: metadata must become equal:
    #
    diff_meta = ''.join(unified_diff( \
                         [x for x in db_main_meta.splitlines() if 'CREATE DATABASE' not in x],
                         [x for x in db_repl_meta.splitlines() if 'CREATE DATABASE' not in x])
                       )
    # Must be EMPTY:
    print(diff_meta)

#--------------------------------------------

def get_repl_log(act_db_main: Action):
    replication_log = act_db_main.home_dir / 'replication.log'
    rlog_lines = []
    with open(replication_log, 'r') as f:
        rlog_lines = f.readlines()

    return rlog_lines

#--------------------------------------------

@pytest.mark.replication
@pytest.mark.version('>=4.0.5')
def test_1(act_db_main: Action,  act_db_repl: Action, db_main_owner: User, capsys):

    # Map for storing mnemonas and details for every FAILED step:
    run_errors_map = {}

    # Obtain full path + filename for DB_MAIN and DB_REPL aliases.
    # NOTE: we must NOT use 'a.db.db_path' for ALIASED databases!
    # It will return '.' rather than full path+filename.
    # Use only con.info.name for that!
    #
    db_info = {}
    for a in (act_db_main, act_db_repl):
        with a.db.connect() as con:
            db_info[a,  'db_full_path'] = con.info.name

    run_errors_map['init_reset'] = reset_replication(act_db_main, act_db_repl, db_info[act_db_main,'db_full_path'], db_info[act_db_repl,'db_full_path'], db_main_owner)

    # Result: owner of db_main_alias = db_main_owner, i.e. NOT 'SYSDBA'

    sql_init = f"""
        set bail on;
        recreate table test (
            id int generated by default as identity constraint test_pk primary key
            ,f01 int
        );

        recreate table t_completed(id int primary key);
        commit;
    """
    act_db_main.isql(switches=['-q', '-user', db_main_owner.name, '-pass', db_main_owner.password], credentials = False, input = sql_init, combine_output = True)
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


    if max(v.strip() for v in run_errors_map.values()):
        # Some problem raised with delivering DDL changes to replica
        pass
    else:

        sql_revoke_access = f"""
            set wng off;
            set list on;
            revoke delete on test from {db_main_owner.name};
            commit;
            select 1 as db_main_privilege_unexp_remains
            from rdb$database
            where exists (
                select 1 from rdb$user_privileges p
                where p.rdb$relation_name = upper('test') and p.rdb$privilege = upper('D')
            );
            commit;
        """
        act_db_main.isql(switches=['-q', '-user', db_main_owner.name, '-pass', db_main_owner.password], credentials = False, input = sql_revoke_access, combine_output = True)
        run_errors_map['db_main_privilege_unexp_remains']  = act_db_repl.stdout # must be EMPTY
        act_db_main.reset()


    if max(v.strip() for v in run_errors_map.values()):
        # Some problem was in just executed statement
        pass
    else:
        ############################################################
        ###  WAIT UNTIL REPLICA APPLY 'REVOKE' PRIVILEGE COMMAND ###
        ############################################################
        # ( a: Action, max_allowed_time_for_wait, ddl_ready_query = '', isql_check_script = '', replica_expected_out = ''):
        chk_repl_sql = f"set list on;select 1 as db_repl_privilege_unexp_remains from rdb$database where exists(select 1 from rdb$user_privileges p where p.rdb$relation_name = upper('test') and p.rdb$privilege = upper('D'));"
        watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query = '', isql_check_script = chk_repl_sql, replica_expected_out = '' )
        # Must be EMPTY:
        run_errors_map['db_repl_privilege_not_deleted'] = capsys.readouterr().out

    # This test changes OWNER of db_main to NON-sysdba.
    # We have to revert this change regardless on test outcome.
    run_errors_map['final_reset'] = reset_replication(act_db_main, act_db_repl, db_info[act_db_main,'db_full_path'], db_info[act_db_repl,'db_full_path'])

    # NO NEEDED because we have done reset just now: drop_db_objects(act_db_main, act_db_repl, capsys)

    if max(v.strip() for v in run_errors_map.values()):
        print(f'Problem(s) detected, check run_errors_map:')
        for k,v in run_errors_map.items():
            if v.strip():
                print(k,':')
                print(v.strip())
                print('-' * 40)
    
    assert '' == capsys.readouterr().out
