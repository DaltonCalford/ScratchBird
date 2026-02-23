#coding:utf-8

"""
ID:          replication.invalid_msg_if_target_db_has_no_replica_flag
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6989
TITLE:       Invalid message in replication.log (and possibly crash in the case of synchronous replication) when the target DB has no its "replica" flag set
DESCRIPTION:
    Test changes replica DB attribute (removes 'replica' flag). Then we do some trivial DDL on master (create and drop table).
    Log of replication must soon contain "ERROR: Database is not in the replica mode".
    If this phrase does not appear during <MAX_TIME_FOR_WAIT_SEGMENT_IN_LOG> seconds then we have a bug.

    Otherwise we continue and return attribute 'replica' to the target DB.
    After this replication log must contain phrase:
        VERBOSE: Segment <N> (<M> bytes) is replicated in <K> ms, deleting
    We can assume that replication finished OK only when such line is found see ('POINT-1').

    Further,  we invoke ISQL with executing auxiliary script for drop all DB objects on master (with '-nod' command switch).
    After all objects will be dropped, we have to wait again until  replica becomes actual with master (see 'POINT-2').

    Finally, we extract metadata for master and replica and compare them (see 'f_meta_diff').
    The only difference in metadata must be 'CREATE DATABASE' statement with different DB names - we suppress it,
    thus metadata difference must not be issued.

FBTEST:      tests.functional.replication.invalid_msg_if_target_db_has_no_replica_flag
NOTES:
    [26.08.2022] pzotov
    1. In case of any errors (somewhat_failed <> 0) test will re-create db_main and db_repl, and then perform all needed
       actions to resume replication (set 'replica' flag on db_repl, enabling publishing in db_main, remove all files
       from subdirectories <repl_journal> and <repl_archive> which must present in the same folder as <db_main>).
    2. Warning raises on Windows and Linux:
       ../../../usr/local/lib/python3.9/site-packages/_pytest/config/__init__.py:1126
          /usr/local/lib/python3.9/site-packages/_pytest/config/__init__.py:1126: 
          PytestAssertRewriteWarning: Module already imported so cannot be rewritten: __editable___firebird_qa_0_17_0_finder
            self._mark_plugins_for_rewrite(hook)
       The reason currently is unknown.

    Checked on 5.0.0.623, 4.0.1.2692 - both CS and SS. Both on Windows and Linux.

    [11.11.2023] pzotov
    Refactored: make code to explain details if any error occurred.
    Put here some functions from other replication-related tests in order to make code simpler.

    Checked on 6.0.0.107, 5.0.0.1264 4.0.4.3009.

    [22.12.2023] pzotov
    Refactored: make test more robust when it can not remove some files from <repl_journal> and <repl_archive> folders.
    This can occurs because engine opens <repl_archive>/<DB_GUID> file every 10 seconds and check whether new segments must be applied.
    Because of this, attempt to drop this file exactly at that moment causes on Windows "PermissionError: [WinError 32]".
    This error must NOT propagate and interrupt entire test. Rather, we must only to log name of file that can not be dropped.

    [23.11.2023] pzotov
    Make final SWEEP optional, depending on setting RUN_SWEEP_AT_END - see $QA_ROOT/files/test_config.ini.

    [21.06.2024] pzotov
    Partially re-implemented:
        * full replacement of func check_repl_log; its new name = 'wait_for_repl_err'
        * added code for simplifying debug;
        * removed check of capsys content from most places (replaced with comparison of result with empty string).
    Checked on Windows, 4.0.5.3112-d2e612c, 5.0.1.1416-b4b3559, 6.0.0.374-0097d28 (SS and CS).
"""
import os
import shutil
import re
from difflib import unified_diff
from pathlib import Path
import time
import datetime as py_dt

import pytest
from firebird.qa import *
from firebird.driver import connect, create_database, DbWriteMode, ReplicaMode, ShutdownMode, ShutdownMethod, DatabaseError


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

substitutions = [('Start removing objects in:.*', 'Start removing objects'),
                 ('Finish. Total objects removed:  [1-9]\\d*', 'Finish. Total objects removed'),
                 ('.* CREATE DATABASE .*', ''),
                 ('REP_BLOB_ID.*', ''),
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

def wait_for_repl_err( act_db_main: Action, replold_lines, max_allowed_time_for_wait):

    replication_log = act_db_main.home_dir / 'replication.log'

    # ERROR: Database is not in the replica mode
    p_database_not_replica = re.compile('ERROR:\\s+Database.* not.* replica', re.IGNORECASE)

    found_required_message = False
    found_required_line = ''
    for i in range(0,max_allowed_time_for_wait):

        time.sleep(1)

        with open(replication_log, 'r') as f:
            diff_data = unified_diff(
                replold_lines,
                f.readlines()
              )

        for k,d in enumerate(diff_data):
            if p_database_not_replica.search(d):
                found_required_message = True
                break

        if found_required_message:
            break

    if not found_required_message:
        # ACHTUNG! This looks weird but we have to either re-read replication log now or wait at least <JOURNAL_ARCHIVE_TIMEOUT> seconds
        # if we want to see FULL (actual) content of this log! Otherwise last part of log will be missed. I have no explanations for that :(
        with open(replication_log, 'r') as f:
            diff_data = unified_diff(
                replold_lines,
                f.readlines()
              )
        unexp_msg = f"Expected ERROR message was not found for {max_allowed_time_for_wait} seconds."
        repllog_diff = '\n'.join( ( ('%4d ' %i) + r.rstrip() for i,r in enumerate(diff_data) ) )
        result = '\n'.join( (unexp_msg, 'Lines in replication.log:', repllog_diff) )
    else:
        result = ''

    return result

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

    return result

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
            #print(capsys.readouterr().out)

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
@pytest.mark.version('>=4.0.1')
def test_1(act_db_main: Action,  act_db_repl: Action, capsys):

    db_info = {}
    out_prep = out_main = out_back = out_drop = out_reset = ''

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    replication_log = act_db_main.home_dir / 'replication.log'
    replold_lines = []
    with open(replication_log, 'r') as f:
        replold_lines = f.readlines()
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
        srv.database.set_replica_mode(database = act_db_repl.db.db_path, mode = ReplicaMode.NONE)

    # Must be EMPTY:
    out_prep = capsys.readouterr().out

    if out_prep:
        # Some problem raised during change DB header(s)
        pass
    else:
        sql_init = '''
            set bail on;
            set list on;
            recreate table test(id int primary key using index test_pk, x int);
            commit;
            insert into test(id, x) values(1, 100);
            commit;
        '''

        # Applying script on master but replica will NOT accept it because its replication mode = NONE.
        # Message "ERROR: Database is not in the replica mode" must appear after this ISQL call:
        #
        act_db_main.isql(switches=['-q'], input = sql_init, combine_output = True)
        out_prep = act_db_main.clean_stdout
        act_db_main.reset()

    if out_prep:
        pass
    else:

        # During next <MAX_TIME_FOR_WAIT_SEGMENT_IN_LOG> seconds message
        # "ERROR: Database is not in the replica mode" must appear in replication log

        ##################################################################
        ###  W A I T    F O R    E R R O R    I N    R E P L . L O G   ###
        ##################################################################
        out_main = wait_for_repl_err( act_db_main, replold_lines, MAX_TIME_FOR_WAIT_SEGMENT_IN_LOG)

        try:
            # Return db_repl mode to 'read-only' (as it was before this test)
            # and wait after it for <MAX_TIME_FOR_WAIT_SEGMENT_IN_LOG> seconds
            # for previously created segment be replicated:
            with act_db_repl.connect_server() as srv:
                srv.database.set_replica_mode(database = act_db_repl.db.db_path, mode = ReplicaMode.READ_ONLY)

            # Query to be used for check that all DB objects present in replica (after last DML statement completed on master DB):
            ddl_ready_query = "select 1 from rdb$relations where rdb$relation_name = upper('test')"

            # Query to be used that replica DB contains all expected data (after last DML statement completed on master DB):
            isql_check_script = """
                set bail on;
                set list on;
                set count on;
                select
                    rdb$get_context('SYSTEM','REPLICA_MODE') replica_mode
                    ,id
                    ,x
                from test;
            """

            isql_expected_out = f"""
                REPLICA_MODE READ-ONLY
                ID 1
                X 100
                Records affected: 1
            """

            ##############################################################################
            ###  W A I T   U N T I L    R E P L I C A    B E C O M E S   A C T U A L   ###
            ##############################################################################
            out_back = watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query, isql_check_script, isql_expected_out)

        except Exception as e:
            out_back = e.__str__()

        # Must be EMPTY:
        out_drop = drop_db_objects(act_db_main, act_db_repl, capsys)


    if [ x for x in (out_prep, out_main, out_back, out_drop) if x.strip() ]:
        # We have a problem either with DDL/DML or with dropping DB objects.
        # First, we have to RECREATE both master and slave databases
        # (otherwise further execution of this test or other replication-related tests most likely will fail):
        out_reset = reset_replication(act_db_main, act_db_repl, db_info[act_db_main,'db_full_path'], db_info[act_db_repl,'db_full_path'])

        # Next, we display out_main, out_drop and out_reset:
        #
        print('Problem(s) detected:')
        if out_prep.strip():
            print('out_prep:')
            print(out_prep)
        if out_main.strip():
            print('out_main:')
            print(out_main)
        if out_back.strip():
            print('out_back:')
            print(out_back)
        if out_drop.strip():
            print('out_drop:')
            print(out_drop)
        if out_reset.strip():
            print('out_reset:')
            print(out_reset)

        assert '' == capsys.readouterr().out
