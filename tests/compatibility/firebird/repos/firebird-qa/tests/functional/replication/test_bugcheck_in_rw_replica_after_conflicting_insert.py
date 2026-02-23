#coding:utf-8

"""
ID:          replication.bugcheck_in_rw_replica_after_conflicting_insert
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8040
TITLE:       Bugcheck 183 (wrong record length) could happen on replica database after UK violation on insert
DESCRIPTION:
    Test temporary changes mode of replica to READ-WRITE.

    We create table 'PERF_AGG' on master with PK-column and UNIQUE constraint for other several columns (<S>)
    After this we wait until replica becomes actual to master, and this delay will last no more then threshold that
    is defined by MAX_TIME_FOR_WAIT_DATA_IN_REPLICA variable (measured in seconds), see QA_ROOT/files/test_config.ini

    Then we change mode of replica to *READ-WRITE* and add record in its 'PERF_AGG' table.
    After this, we add record in master 'PERF_AGG' with same values in <S> (but value in PK differs from replica one).
    This causes violation of UNIQUE index in replica DB but no error must raise (UPDATE should be with warning).
    We monintor replication.log until appropriate message appear in it.

NOTES:
    [14.03.2024] pzotov
    ::: ACHTUNG :::
    If test runs against FB service then make sure that its recovery option does NOT set to 'Restart'!
    Otherwise every FB restart will cause bugcheck at first connection to replica DB with infinite restarts.

    Added temporary mark 'disabled_in_forks' to SKIP this test when QA runs agains *fork* of standard FB.
    Reason: infinite bugchecks and possible disk overflow if dumps creation enabled.
    Confirmed problem and bugcheck on:
        6.0.0.286 (12.03.2024), 5.0.1.1358 (13.03.2024), 4.0.5.3066  (regular sapshot, date: 13.03.2024):
        E firebird.driver.types.DatabaseError: Unable to complete network request to host "localhost".
        E -Failed to establish a connection.
    Checked on Windows:
        6.0.0.288 eee5704, 5.0.1.1360 055b53b, 4.0.5.3077 (regular sapshot, date: 14.03.2024)

    [26.03.2024] pzotov
    1. BEFORE 6.0.0.293 (commit #62f4c5a7, "Improvement #8042 : Improve conflict resolution...") we had to wait until
       message "ERROR: attempt to store duplicate value" will appear TWO times (it proved that there was no bugcheck).
       SINCE 6.0.0.293 error does not raise. Rather, such data (with duplicated values for unique index) will cause
       "Record being inserted ... already exists, updating instead".

    2. Before commit #62f4c5a7 (fixed #8042), replication was no longer viable at the end of this test. Because of this,
       we could not call drop_db_objects() function on master with assumption that changes will be transferred to replica.
       This caused implementation of 'special version' of reset_replication() function: it immediately changes DB state
       to 'full shutdown' for both databases and drop them.
       Although this function can be replaced now to its 'usual' version (see another tests), I've decided to keep
       its previous code (i.e. to keep the way it is).

    Checked on 6.0.0.299 b1ba859 (SS/CS).
    Checked on 4.0.5.3112-d2e612c, 5.0.1.1416-b4b3559, 6.0.0.374-0097d28
"""
import os
import shutil
from difflib import unified_diff
from pathlib import Path
import re
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

        # xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
        # xxx   s h u t d o w n    a n d    d r o p     m a s t e r    a n d    r e p l i c a   xxx
        # xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
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


        # xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
        # xxx   d e l e t e    s e g m e n t s   xxx
        # xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
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

def check_repl_log( act_db_main: Action, max_allowed_time_for_wait, expected_output_str ):

    replication_log = act_db_main.home_dir / 'replication.log'

    init_replication_log = []
    with open(replication_log, 'r') as f:
        init_replication_log = f.readlines()

    # firebird.log must NOT contain line:
    # "internal Firebird consistency check (wrong record length (183), file: vio.cpp line: 1885)"
    # Following three lines must appear the replication.log for <max_allowed_time_for_wait> seconds:
    # ------------------------------------------
	# VERBOSE: Added 1 segment(s) to the queue
	# WARNING: Record being inserted into table PERF_AGG already exists, updating instead
	# VERBOSE: Segment 2 (447 bytes) is replicated in 0.016s, deleting
    # ------------------------------------------
    # Otherwise it means that we have a problem:

    result = ''
    with act_db_main.db.connect(no_db_triggers = True) as con:
        with con.cursor() as cur:
            cur.execute("select rdb$get_context('SYSTEM','REPLICATION_SEQUENCE') from rdb$database")
            last_generated_repl_segment = int(cur.fetchone()[0])
    
            ptn_repl_chk_01 = re.compile('Added \\d+ segment\\(s\\) to the queue')
            ptn_repl_chk_02 = re.compile('Record being inserted .* exists, updating instead')
            ptn_repl_chk_03 = re.compile(f'Segment {last_generated_repl_segment} \\(\\d+ bytes\\) is replicated .* deleting')
            line_match_01 = set()
            line_match_02 = set()
            line_match_03 = set()

            found_all = False

            #	for i in range(0,max_allowed_time_for_wait):
            for i in range(0,max_allowed_time_for_wait):

                time.sleep(1)

                # Get content of fb_home/replication.log _after_ isql finish:
                with open(replication_log, 'r') as f:
                    diff_data = [x.strip() for x in unified_diff(init_replication_log,f.readlines())]

                for k,diff_line in enumerate(diff_data):
                    expr = f"select {k} as iter, current_timestamp, q'<{diff_line}>' from rdb$database"
                    if ptn_repl_chk_01.search(diff_line):
                        line_match_01.add(k)
                        expr = f"select {k} as info_01_line, q'<{diff_line}>' as info_01_text, {len(line_match_01)} as info_01_found, {len(line_match_02)} as info_02_found, {len(line_match_03)} as info_03_found from rdb$database"
                    if ptn_repl_chk_02.search(diff_line):
                        line_match_02.add(k)
                        expr = f"select {k} as info_02_line, q'<{diff_line}>' as info_02_text, {len(line_match_02)} as info_02_found, {len(line_match_02)} as info_02_found, {len(line_match_03)} as info_03_found from rdb$database"
                    if ptn_repl_chk_03.search(diff_line):
                        line_match_03.add(k)
                        expr = f"select {k} as info_03_line, q'<{diff_line}>' as info_03_text, {len(line_match_03)} as info_03_found, {len(line_match_02)} as info_02_found, {len(line_match_03)} as info_03_found from rdb$database"

                    # _dummy_ = cur.execute(expr).fetchone()
                    if len(line_match_01) >= 1 and len(line_match_02) >= 1 and len(line_match_03) >= 1:
                        found_all = True
                        break
                        
                if found_all:
                    break
            
            if not found_all:
                unexp_msg = f'UNEXPECTED: messages about replicated segment {last_generated_repl_segment} did not appear for {max_allowed_time_for_wait} seconds.'
                repllog_diff = '\n'.join( [ ('%4d ' %i) + r  for i,r in enumerate(diff_data) ] )
                checked_ptn_msg = f'{ptn_repl_chk_01=}\n{ptn_repl_chk_02=}\n{ptn_repl_chk_03=}'
                lines_match_msg = f'{line_match_01=}; {line_match_02=}; {line_match_03=}; '
                result = '\n'.join( [unexp_msg, 'Lines in replication.log:', repllog_diff, 'Checked patterns:', checked_ptn_msg, 'Lines NN that match patterns:', lines_match_msg] )
            else:
                result = expected_output_str
            
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
        print( f'UNEXPECTED. Initial check query did not return any rows for {max_allowed_time_for_wait} seconds.' )
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

@pytest.mark.disabled_in_forks
@pytest.mark.replication
@pytest.mark.version('>=4.0.5')
def test_1(act_db_main: Action,  act_db_repl: Action, capsys):


    out_prep, out_main, out_drop = '', '', ''
    # Obtain full path + filename for DB_MAIN and DB_REPL aliases.
    # NOTE: we must NOT use 'a.db.db_path' for ALIASED databases!
    # It will return '.' rather than full path+filename.
    # Use only con.info.name for that!
    #
    db_info = {}
    for a in (act_db_main, act_db_repl):
        with a.db.connect(no_db_triggers = True) as con:
            db_info[a,  'db_full_path'] = con.info.name

    with act_db_repl.connect_server() as srv:
        srv.database.set_replica_mode(database = act_db_repl.db.db_path, mode = ReplicaMode.READ_WRITE)


    # Must be EMPTY:
    out_prep = capsys.readouterr().out
    if out_prep:
        # Some problem raised during change DB header(s)
        pass
    else:
        sql_init = '''
            recreate table perf_agg (
                 unit varchar(80) character set utf8
                ,exc_unit char(1)
                ,fb_gdscode integer
                ,dts_interval integer
                ,id bigint generated by default as identity not null
                ,constraint pk_perf_agg primary key (id)
            );
            create unique index perf_agg_unq on perf_agg (unit, fb_gdscode, exc_unit, dts_interval);
            commit;
        '''

        act_db_main.isql(switches=['-q'], input = sql_init, combine_output = True)
        out_prep = act_db_main.clean_stdout
        act_db_main.reset()

    if out_prep:
        # Some problem raised during init_sql execution
        pass
    else:
        # Query to be used for check that all DB objects present in replica (after last DML statement completed on master DB):
        ddl_ready_query = "select 1 from rdb$indices where rdb$index_name = upper('perf_agg_unq')"
        ##############################################################################
        ###  W A I T   U N T I L    R E P L I C A    B E C O M E S   A C T U A L   ###
        ##############################################################################
        watch_replica( act_db_repl, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, ddl_ready_query)
        # Must be EMPTY:
        out_prep = capsys.readouterr().out

    if out_prep:
        # Some problem raised during initial DDL + DML replication 
        pass
    else:
        expected_parsing_result = 'FOUND_EXPECTED_COUNT_OF_ERROR_MESSAGES'
        try:
            dml_sttm = "insert into perf_agg(unit,fb_gdscode,exc_unit,dts_interval,id) values (?, ? , ? , ? , ?)"
            
            with act_db_repl.db.connect() as con_repl:
                with con_repl.cursor() as cur_repl:
                    cur_repl.execute(dml_sttm, ('sp_client_order', None, None, 10, -1))
                con_repl.commit()

            with act_db_main.db.connect() as con_main:
                with con_main.cursor() as cur_main:
                    cur_main.execute(dml_sttm, ('sp_client_order', None, None, 10, 1))
                con_main.commit()

            ###############################################################
            ###  W A I T   F O R   E R R O R    I N   R E P L . L O G   ###
            ###############################################################
            actual_result = check_repl_log(act_db_main, MAX_TIME_FOR_WAIT_DATA_IN_REPLICA, expected_parsing_result)
            if actual_result != expected_parsing_result:
                out_main = actual_result

        except Exception as e:
            out_main = e.__str__()

    ########################################################################

    
    # ::: NB :::
    # Replica DB now is in a state that does not allow its modification by any segment that comes from master.
    # Thus it is *useless* to call drop_db_objects() which changes master and then waits until replica will accept appropriate
    # segments and become identical to master -- this will NEVER be in our case!
    # Because of that, we have to RESET replication immediatelly, i.e. drop both databases and segments + create databases again:
    #
    out_reset = reset_replication(act_db_main, act_db_repl, db_info[act_db_main,'db_full_path'], db_info[act_db_repl,'db_full_path'])


    if [ x for x in (out_prep, out_main, out_drop, out_reset) if x.strip() ]:
        # We have a problem either with DDL/DML.
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
