#coding:utf-8

"""
ID:          replication.dblevel_triggers_must_not_fire_on_replica
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6850
TITLE:       Replica DB must not fire DB-level triggers but their activity on master must be eventually seen in replica
DESCRIPTION:
    Test creates five DB-level triggers in the master DB (on connect/disconnect; on tx start/commit/rollback).
    Each of them perform registration of action in the table with name 'log_db_triggers_activity'.
    We make test connect to master DB to fire these triggers, and this causes adding records to log_db_triggers_activity.
    DB-level triggers must NOT fire in replica DB (because its is read-only) but metadata and content of log_db_triggers_activity
    must be transferred to replica using segments.

    After this we wait until replica becomes actual to master, and this delay will last no more then threshold that
    is defined by MAX_TIME_FOR_WAIT_DATA_IN_REPLICA variable (measured in seconds), see QA_ROOT/files/test_config.ini

    Check is performed by querying DB replica, see call of watch_replica() function.
    Then we check that both databases have same data in the user table 'test'.

    Further, we invoke ISQL with executing auxiliary script for drop all DB objects on master (with '-nod' command switch).
    After all objects will be dropped, we have to wait again until replica becomes actual with master.
    Check that both DB have no custom objects is performed (see UNION-ed query to rdb$ tables + filtering on rdb$system_flag).

    Finally, we extract metadata for master and replica and make comparison.
    The only difference in metadata must be 'CREATE DATABASE' statement with different DB names - we suppress it,
    thus metadata difference must not be issued.

FBTEST:      tests.functional.replication.dblevel_triggers_must_not_fire_on_replica
NOTES:
    [25.08.2022] pzotov
    1. In case of any errors (somewhat_failed <> 0) test will re-create db_main and db_repl, and then perform all needed
       actions to resume replication (set 'replica' flag on db_repl, enabling publishing in db_main, remove all files
       from subdirectories <repl_journal> and <repl_archive> which must present in the same folder as <db_main>).
    2. Warning raises on Windows and Linux:
       ../../../usr/local/lib/python3.9/site-packages/_pytest/config/__init__.py:1126
          /usr/local/lib/python3.9/site-packages/_pytest/config/__init__.py:1126: 
          PytestAssertRewriteWarning: Module already imported so cannot be rewritten: __editable___firebird_qa_0_17_0_finder
            self._mark_plugins_for_rewrite(hook)
       The reason currently is unknown.

    [15.04.2023] pzotov
    Test was fully re-implemented. We have to query replica DATABASE for presense of data that we know there must appear.
    We have to avoid query of replication log - not only verbose can be disabled, but also because code is too complex.

    We use 'assert' only at the final point of test, with printing detalization about encountered problem(s).
    During all previous steps, we only store unexpected output to variables, e.g.: out_main = capsys.readouterr().out etc.

    [18.07.2023] pzotov
    ENABLED execution of on Linux when ServerMode = Classic after letter from dimitr 13-JUL-2023 12:58.
    See https://github.com/FirebirdSQL/firebird/commit/9aaeab2d4b414f06dabba37e4ebd32587acd5dc0

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
import shutil
import re
from difflib import unified_diff
from pathlib import Path
import time

import pytest
from firebird.qa import *
from firebird.driver import connect, create_database, DbWriteMode, ReplicaMode, ShutdownMode, ShutdownMethod, DatabaseError

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
        print( f'UNEXPECTED. Initial check query did not return any rows for {max_allowed_time_for_wait} seconds.' )
        print('Initial check query:')
        print(ddl_ready_query)
        return
    
    final_check_pass = False
    if isql_check_script:
        for i in range(max_allowed_time_for_wait):
            a.reset()
            a.expected_stdout = replica_expected_out
            a.isql(switches=['-q', '-nod'], input = isql_check_script, combine_output = False)
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
            print(f'Waited for {i} seconds')
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

@pytest.mark.replication
@pytest.mark.version('>=4.0.1')
def test_1(act_db_main: Action,  act_db_repl: Action, capsys):


    out_prep, out_main, out_drop = '', '', ''

    # Obtain full path + filename for DB_MAIN and DB_REPL aliases.
    # NOTE: we must NOT use 'a.db.db_path' for ALIASED databases!
    # It will return '.' rather than full path+filename.
    # Use only con.info.name for that!
    #    
    db_main_file, db_repl_file = '', ''

    with act_db_main.db.connect(no_db_triggers = True) as con:
        #if act_db_main.vars['server-arch'] == 'Classic' and os.name != 'nt':
        #    pytest.skip("Waiting for FIX: 'Engine is shutdown' in replication log for CS. Linux only.")

        db_main_file = con.info.name

    with act_db_repl.db.connect(no_db_triggers = True) as con:
        db_repl_file = con.info.name

    # Must be EMPTY:
    out_prep = capsys.readouterr().out

    sql_init = '''
        set bail on;
        set term ^;
        execute block as
        begin
            -- Define context variable in order to prevent
            -- DB-level triggers from firing during this execution:
            rdb$set_context('USER_SESSION', 'SKIP_DBLEVEL_TRG','1');
        end
        ^
        set term ;^

        -- ::: NB :::
        -- We can not start this script from 'zero-point', i.e. 'create table ...; create view ... ;' etc,
        -- because it will fail if master or replica DB contain some objects which could remain there
        -- due to fail of some previous test which also had deal with replication and used these databases.
        -- Here we must remove all dependencies and only after this required DB objects can be recreated:
        create or alter trigger trg_tx_start inactive on transaction start as begin end;
        create or alter trigger trg_tx_commit inactive on transaction commit as begin end;
        create or alter trigger trg_tx_rollback inactive on transaction rollback as begin end;
        create or alter trigger trg_connect inactive on connect as begin end;
        create or alter trigger trg_disconnect inactive on disconnect as begin end;
        create or alter procedure sp_log_dblevel_trg_event as begin end;
        create or alter view v_log_db_triggers_activity as select 1 x from rdb$database;
        commit;

        -- result: no more objects that depend on table 'log_db_triggers_activity', now we can recreate it.

        recreate table log_db_triggers_activity (
            id int generated by default as identity constraint pk_log_db_triggers_activity primary key
            ,dts timestamp default 'now'
            ,att integer default current_connection
            ,trn integer default current_transaction
            ,app varchar(80)
            ,evt varchar(80)
        );

        create or alter view v_log_db_triggers_activity as select * from log_db_triggers_activity;

        set term ^;
        create or alter procedure sp_log_dblevel_trg_event (
            a_event_type varchar(80) -- type of column log_db_triggers_activity.evt
           ,a_working_tx int default null
        )
        as
            declare v_app varchar(255);
            declare p smallint;
            declare back_slash char(1);
        begin
            v_app = reverse( right(rdb$get_context('SYSTEM','CLIENT_PROCESS'), 80) );
            back_slash = ascii_char(92); -- backward slash; do NOT specify it literally otherwise Python will handle it as empty string!
            p = maxvalue(position(back_slash in v_app ), position('/' in v_app ));
            v_app = reverse(substring(v_app from 1 for p-1));
            execute statement( 'insert into v_log_db_triggers_activity( trn, app, evt) values( ?, ?, ? )' ) ( coalesce(:a_working_tx, current_transaction), :v_app, :a_event_type) ;

        end
        ^

        create or alter trigger trg_tx_start active on transaction start as
        begin
            if (rdb$get_context('USER_SESSION', 'SKIP_DBLEVEL_TRG') is null ) then
                -- execute procedure sp_log_dblevel_trg_event( 'TX_START, TIL=' || coalesce( rdb$get_context('SYSTEM', 'ISOLATION_LEVEL'), '[null]' ) );
                execute procedure sp_log_dblevel_trg_event( 'TX_START' );
        end
        ^

        create or alter trigger trg_tx_commit active on transaction commit as
        begin
            if (rdb$get_context('USER_SESSION', 'SKIP_DBLEVEL_TRG') is null ) then
                -- execute procedure sp_log_dblevel_trg_event( 'TX_COMMIT, TIL=' || coalesce( rdb$get_context('SYSTEM', 'ISOLATION_LEVEL'), '[null]' ) );
                execute procedure sp_log_dblevel_trg_event( 'TX_COMMIT' );
        end
        ^

        create or alter trigger trg_tx_rollback active on transaction rollback as
            declare v_current_tx int;
        begin
            v_current_tx = current_transaction;
            if (rdb$get_context('USER_SESSION', 'SKIP_DBLEVEL_TRG') is null ) then
                in autonomous transaction do
                -- execute procedure sp_log_dblevel_trg_event( 'TX_ROLLBACK, TIL=' || coalesce( rdb$get_context('SYSTEM', 'ISOLATION_LEVEL'), '[null]' ), v_current_tx );
                execute procedure sp_log_dblevel_trg_event( 'TX_ROLLBACK' );
        end
        ^

        create or alter trigger trg_connect active on connect position 0 as
        begin
            if (rdb$get_context('USER_SESSION', 'SKIP_DBLEVEL_TRG') is null ) then
                execute procedure sp_log_dblevel_trg_event( 'DB_ATTACH' );
        end
        ^

        create or alter trigger trg_disconnect active on disconnect position 0 as
            declare v_current_tx int;
        begin
            if (rdb$get_context('USER_SESSION', 'SKIP_DBLEVEL_TRG') is null ) then
                execute procedure sp_log_dblevel_trg_event( 'DB_DETACH');
        end
        ^
        set term ;^
        commit;
    '''

    act_db_main.isql(switches=['-q', '-nod'], input = sql_init, combine_output = True)
    out_prep = act_db_main.clean_stdout
    act_db_main.reset()

    if out_prep:
        pass
    else:

        # Test connect to master DB, just to fire DB-level triggers:
        ###########################
        with act_db_main.db.connect() as con:
            cur = con.cursor()
            for i in (1,2):
                for r in cur.execute(f'select {i} from rdb$database'):
                    pass
                if i == 1:
                    con.commit()
                else:
                    con.rollback()

        # Query to be used for check that all DB objects present in replica (after last DDL statement completed on master DB):
        ddl_ready_query = "select 1 from rdb$triggers rt where rt.rdb$trigger_name = upper('trg_disconnect') and coalesce(rt.rdb$trigger_inactive,0) = 0"

        # Query to be used that replica DB contains all expected data (after last DML statement completed on master DB):
        isql_check_script = f"""
                set list on;
                set count on;
                select evt as trg_name
                from v_log_db_triggers_activity order by id;
        """

        isql_expected_out = """
            TRG_NAME DB_ATTACH
            TRG_NAME TX_START
            TRG_NAME TX_COMMIT
            TRG_NAME TX_START
            TRG_NAME TX_ROLLBACK
            TRG_NAME TX_COMMIT
            TRG_NAME DB_DETACH
            Records affected: 7
        """

        ##############################################################################
        ###  W A I T   U N T I L    R E P L I C A    B E C O M E S   A C T U A L   ###
        ##############################################################################
        watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query, isql_check_script, isql_expected_out)
        # Must be EMPTY:
        out_main = capsys.readouterr().out

    drop_db_objects(act_db_main, act_db_repl, capsys)
    # Must be EMPTY:
    out_drop = capsys.readouterr().out

    if [ x for x in (out_prep, out_main, out_drop) if x.strip() ]:
        # We have a problem either with DDL/DML or with dropping DB objects.
        # First, we have to RECREATE both master and slave databases
        # (otherwise further execution of this test or other replication-related tests most likely will fail):
        out_reset = reset_replication(act_db_main, act_db_repl, db_main_file, db_repl_file)

        # Next, we display out_main, out_drop and out_reset:
        #
        print('Problem(s) detected:')
        if out_prep.strip():
            print('out_prep:')
            print(out_prep)
        if out_main.strip():
            print('out_main:')
            print(out_main)
        if out_drop.strip():
            print('out_drop:')
            print(out_drop)
        if out_reset.strip():
            print('out_reset:')
            print(out_reset)

        assert '' == capsys.readouterr().out

