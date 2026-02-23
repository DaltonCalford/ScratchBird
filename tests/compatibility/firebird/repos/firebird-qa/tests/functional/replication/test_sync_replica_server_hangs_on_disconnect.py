#coding:utf-8

"""
ID:          replication.test_sync_replica_server_hangs_on_disconnect
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6727
TITLE:       Synchronous replication hangs on disconnect [CORE6497]
DESCRIPTION:
    Test makes actions specified in the ticket.
    In order to have ability to cancel hanging client, we create temporary Python script
    and invoke it using subprocess.run() with 'timeout' argument ('MAX_WAIT_FOR_FINISH').
    If all steps passed OK, we can verify content of replica DB by query to the table
    hat was created on master and should be replicated.
    Finally, we have to drop table on master and check that metadata on both databases
    are identical (w/o 'create database' statement).

    Outcome is considered as 'passed' only if:
    1) no errors occurred during all above-mentioned steps;
    2) temporary Python script (that was launched with timeout arg.) completed normally,
       i.e. without subprocess.TimeoutExpired or some other exception
    3) replica DB is synchronized with master, i.e. it *contains* table and data that was added on master.
NOTES:
    [18.02.2026] pzotov
    Test uses two pre-created DBs for Sync replication that must present in $(dir_sampleDb)/qa_replication/
    and have following file names: 'db_sync_main.fdb'; 'db_sync_repl.fdb' (QA-scenarios must create them).

    File $QA_HOME/files/test_config.ini must have following definitions:
        # values must match to specified in the databases.conf:
        DB_SYNC_MAIN_ALIAS = db_sync_main_alias
        DB_SYNC_REPL_ALIAS = db_sync_REPL_alias

        # see in qa-replicaton.conf, 'sync_replica' sub-section: name of files for storing username and password
        # for connection to replica DB during Sync replication. Do not add any directory to these values:
        # 
        SYNC_REPL_USER_FILE = sync_repl_user.txt
        SYNC_REPL_PSWD_FILE = sync_repl_pswd.txt

    File $FB_HOME/databases.conf must have aliases: db_sync_main_alias; db_sync_repl_alias
    File $FB_HOME/replication.conf must define following parameters for replication:
        database = $(dir_sampleDb)/qa_replication/db_sync_main.fdb
        {
            sync_replica = localhost:$(dir_sampleDb)/qa_replication/db_sync_repl.fdb
            {
                username_file = $(dir_sampleDb)/qa_replication/sync_repl_user.txt
                password_file = $(dir_sampleDb)/qa_replication/sync_repl_pswd.txt
            }
        }
    
    ::: NB :::
    This bug was found on version that could NOT recognize known macroses in replication.conf, specially - $(dir_sampleDb).
    To reproduce problem on such versions, one need to prepare replication.conf using absolute existing directories, e.g.:
        database = /var/tmp/db_sync_main.fdb
        {
            sync_replica = localhost:/var/tmp/db_sync_repl.fdb
            ...
        }

    FB started to recognize macroses in replication.conf sice 08-sep-2022 (see #7294).
    Fix was 24-feb-2021 20:14 in v4.0-release (that was 'master' at that time):
    https://github.com/FirebirdSQL/firebird/commit/f18079af4d359f87698884bd6164ffa1dbe89ac3

    Confirmed bug on 4.0.0.2372 (21-feb-2021): client hangs, server loads CPU for ~100%.
    Checked on 4.0.0.2377 (27-feb-2021).
    Checked on 6.0.0.1456-0-789d467; 5.0.4.1765-2c1e56d; 4.0.7.3243-ea1c15c5.
"""
import sys
import time
import re
import locale
import subprocess
from difflib import unified_diff
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import * 

########################
MAX_WAIT_FOR_FINISH = 15
########################

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
repl_settings = QA_GLOBALS['replication']

MAIN_DB_ALIAS = repl_settings['db_sync_main_alias']
REPL_DB_ALIAS = repl_settings['db_sync_repl_alias']

db_main = db_factory( filename = '#' + MAIN_DB_ALIAS, do_not_create = True, do_not_drop = True)
db_repl = db_factory( filename = '#' + REPL_DB_ALIAS, do_not_create = True, do_not_drop = True)

substitutions = [ ('[\t ]+', ' '), ]
act_db_main = python_act('db_main', substitutions=substitutions)
act_db_repl = python_act('db_repl', substitutions=substitutions)

tmp_py = temp_file('tmp_gh_6727.py')
tmp_log = temp_file('tmp_7979.log')

#--------------------------------------------

def reset_sync_replication(act_db_main: Action, act_db_repl: Action, db_main_file, db_repl_file):
    out_reset = ''
    failed_shutdown_db_map = {} # K = 'db_main', 'db_repl'; V = error that occurred when we attempted to change DB state to full shutdown (if it occurred)

    with act_db_main.connect_server() as srv:

        # !! IT IS ASSUMED THAT REPLICATION FOLDERS ARE IN THE SAME DIR AS <DB_MAIN> !!
        # DO NOT use 'a.db.db_path' for ALIASED database!
        # It will return '.' rather than full path+filename.

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
                Path(f).unlink()
            except DatabaseError as e:
                failed_shutdown_db_map[ f ] = e.__str__()


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

def get_repl_log(act_db_main: Action):
    replication_log = act_db_main.home_dir / 'replication.log'
    rlog_lines = []
    with open(replication_log, 'r') as f:
        rlog_lines = f.readlines()

    return rlog_lines

#--------------------------------------------

def remove_excessive_leading_spaces( input_txt ):

    # Removes uneeded leading spaces from multi-line text, but with preserving indentation that was originally created.
    min_indent_to_preserve = min([len(x)-len(x.lstrip()) for x in input_txt.splitlines() if x.strip()])
    return '\n'.join( [ x[ min_indent_to_preserve : ].rstrip() for x in input_txt.splitlines() if x.strip()] )

#--------------------------------------------

@pytest.mark.replication
@pytest.mark.version('>=4.0')
def test_1(act_db_main: Action,  act_db_repl: Action, tmp_py: Path, tmp_log: Path, capsys):

    # Map for storing mnemonas and details for every FAILED step:
    run_errors_map = {}

    # Obtain full path + filename for DB_MAIN and DB_REPL aliases.
    # NOTE: we must NOT use 'a.db.db_path' for ALIASED databases!
    # It will return '.' rather than full path+filename.
    # Use only con.info.name for that!
    #
    db_info = {}
    log_before = []
    replold_lines = []
    try:
        for a in (act_db_main, act_db_repl):
            with a.db.connect() as con:
                db_info[a, 'db_full_path'] = con.info.name
        # Initial content of firebird.log and replicaion.log:
        log_before = act_db_main.get_firebird_log()
        replold_lines = get_repl_log(act_db_main)
    except DatabaseError as e:
        # Example when some of aliases is defined incorrectly (Linux):
        #  I/O error during "open" operation for file "db_sync_REPL_alias"
        # -Error while trying to open file
        # -No such file or directory\ngdscodes:\n(335544344, 335544734)
        run_errors_map['init_db_err'] = '\n'.join( (e.__str__(), 'gdscodes:', f'{e.gds_codes}') )
    except Exception as e:
        run_errors_map['init_common_err'] = f"Unexpected error during initial code execution: {e}"

    assert len(run_errors_map)==0 and len(db_info) == 2

    #------------------------------------------------------------

    if len(run_errors_map) == 0:
        tmp_check_py = f"""
            #coding:utf-8

            import argparse as ap
            from pathlib import Path
            from firebird.driver import *
             
            parser = ap.ArgumentParser()
            parser.add_argument("fb_clnt", help="Path to FB client library")
            parser.add_argument("db_main_alias", nargs='?', default='{MAIN_DB_ALIAS}', help="Path or alias for master DB.")
            parser.add_argument("db_repl_alias", nargs='?', default='{REPL_DB_ALIAS}', help="Path or alias for replica DB.")
            parser.add_argument("dba_user", nargs='?', default='{act_db_main.db.user}', help="Login  to connect")
            parser.add_argument("dba_pswd", nargs='?', default='{act_db_main.db.password}', help="Password to connect")
            args = parser.parse_args()

            assert Path(args.fb_clnt).is_file(), f"FB client library not found: {{args.fb_clnt}}"

            dsn_main ='inet://' + args.db_main_alias
            dsn_repl ='inet://' + args.db_repl_alias

            #driver_config.fb_client_library.value = r"{act_db_main.vars['fbclient']}"
            driver_config.fb_client_library.value = args.fb_clnt

            con_repl = connect(dsn_repl, user = args.dba_user, password = args.dba_pswd)
            con_main = connect(dsn_main, user = args.dba_user, password = args.dba_pswd)
            con_main.execute_immediate('recreate table test(id int primary key using index test_pk, x int)')
            con_main.commit()
            con_main.execute_immediate('insert into test(id, x) values(1,100)')
            con_main.commit()
            con_repl.close()
            con_main.close() # <<< at this point client could not return control to OS, server hanged with loading 100% CPU.

            with connect(dsn_repl, user = args.dba_user, password = args.dba_pswd) as con:
                cur = con.cursor()
                try:
                    cur.execute('select d.mon$replica_mode, t.* from mon$database d left join test t on 1=1')
                    ccol=cur.description
                    for r in cur:
                        for i in range(0,len(ccol)):
                            print( ccol[i][0],':', r[i])
                except DatabaseError as e:
                    print(e.__str__())
                    print(e.gds_codes)

            #with connect(dsn_main, user = args.dba_user, password = args.dba_pswd) as con:
            #    con.execute_immediate('drop table test')
            #    con.commit()
        """

        tmp_py.write_text(remove_excessive_leading_spaces(tmp_check_py))

        with open(tmp_log, 'w') as f:
            try:
                ######################################################################
                ###   c a l l    c h i l d    .p y     w i t h     t i m e o u t   ###
                ######################################################################
                py_aux_pid =  subprocess.run( [ sys.executable, '-u', tmp_py, act_db_main.vars['fbclient'] ], stdout = f, stderr = subprocess.STDOUT, timeout = MAX_WAIT_FOR_FINISH )
                if py_aux_pid.returncode != 0:
                    run_errors_map['ret_code'] = f'Script FAILED: retcode={py_aux_pid.returncode}.'
            except subprocess.TimeoutExpired as e:
                # On snapshot 4.0.0.2372 (where bug did exists) control must come here with raising:
                #  Command '['...python.exe', '-u', '...tmp_gh_6727.py', '...fbclient.dll']' timed out after <MAX_WAIT_FOR_FINISH> seconds
                run_errors_map['err_timeout'] = e.__str__()
            except Error as x:
                run_errors_map['err_others'] = x.__str__()

        with open(tmp_log, 'r') as f:
            print(f.read())

    # len(run_errors_map) == 0

    if len(run_errors_map) == 0:
        db_main_meta = act_db_main.extract_meta(charset = 'utf8', io_enc = 'utf8')
        db_repl_meta = act_db_repl.extract_meta(charset = 'utf8', io_enc = 'utf8')

        # Final point: metadata must become equal:
        #
        diff_meta = ''.join(unified_diff( \
                             [x for x in db_main_meta.splitlines() if 'CREATE DATABASE' not in x],
                             [x for x in db_repl_meta.splitlines() if 'CREATE DATABASE' not in x])
                           )
        # Must be EMPTY:
        if diff_meta:
            run_errors_map['diff_meta'] = diff_meta


    # NOT HERE! --> WRONG --> run_errors_map['final_reset'] = reset_sync_replication(act_db_main, act_db_repl, db_info[act_db_main,'db_full_path'], db_info[act_db_repl,'db_full_path'])

    if len(run_errors_map) > 0 and max(v.strip() for v in run_errors_map.values()):
        print(f'Problem(s) detected, check run_errors_map:')
        for k,v in run_errors_map.items():
            if v.strip():
                print(k,':')
                print(v.strip())
                print('-' * 40)
 
    act_db_main.expected_stdout = """
        MON$REPLICA_MODE : 1
        ID : 1
        X : 100
    """
    act_db_main.stdout = capsys.readouterr().out

    assert act_db_main.clean_stdout == act_db_main.clean_expected_stdout

    # must be at LAST point, after assert:
    reset_sync_replication(act_db_main, act_db_repl, db_info[act_db_main,'db_full_path'], db_info[act_db_repl,'db_full_path'])
