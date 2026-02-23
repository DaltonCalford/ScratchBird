#coding:utf-8

"""
ID:          replication.test_sync_replica_allow_spaces_in_username_or_password
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/979f46b71e519d48e24bfe204bad063c39596518
TITLE:       Leading, trailing or inner space character specified in 'sync_replica' must not cause error
DESCRIPTION:
    Test verifies that username_file and/or password_file (defined in 'sync_replica' parameters from replication.conf)
    may contain data with space characters and  this will not stop replication because of authentification problems.
    We define two lists with username  and password, where each element has at least one space character,
    see variables: 'SYNC_USER_NAME_LST' and 'SYNC_USER_PSWD_LST'.
    Every combination of user/password value is checked then (via replacements in the files defined in 'sync_replica'
    parameters from replication.conf.

    Outcome is considered as 'passed' only if:
    1) no errors occurred during creation of appropriate user (who will perform replication) with granting to him role
       with necessary privileges (IGNORE_DB_TRIGGERS, REPLICATE_INTO_DATABASE); this role is granted as DEFAULT for him;
    2) DDL and DML is successfully replicated,i.e. we will able to see its result on replica-DB;
    3) no messages containing 'user name and password are not defined' or 'error occurred during login'
       appeared in firebird.log during this test run;
    4) no error messages appeared in replication.log during this test run.
NOTES:
    [25.01.2026] pzotov
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
    Bug can be reproduced only if replication uses FILES with user/password to be used instead of direct
    specifying such data in 'sync_replica' value. Othwerwise (at least on Windows), space characters will
    be either processed w/o problems (but on snapshot where error must occur) or cause replication stop.
    For example, following:
    1) sync_replica = S. O'Connor:I'm O'Connor@localhost:$(dir_sampleDb)/...
       -- will NOT raise any error on 6.0.0.599-ba58842 (which had no fix)
    2) sync_replica = S. O'Connor   :qwe123@localhost:$(dir_sampleDb)/...
       -- will stop replication on 6.0.0.599-979f46b with "ERROR: Your user...password are not defined"

    (letters to dimitr 19.01.2026 09:20, 11:48)

    Confirmed bug on 6.0.0.599-ba58842 (25.01.2025): replication could not run if user/password contain space(s).
    Checked on 6.0.0.599-979f46b (snapshot based on fix, timestamp: 27.01.2025 1142).
    Checked on 6.0.0.1395 (Windows and Linux).
"""
import re
import locale
from difflib import unified_diff
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import * 

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
repl_settings = QA_GLOBALS['replication']

MAIN_DB_ALIAS = repl_settings['db_sync_main_alias']
REPL_DB_ALIAS = repl_settings['db_sync_repl_alias']
SYNC_REPL_USER_FILE = repl_settings['sync_repl_user_file']
SYNC_REPL_PSWD_FILE = repl_settings['sync_repl_pswd_file']

SYNC_USER_NAME_LST = [' DBA_HELPER', 'DBA_HELPER ', 'dba helper', "  J.   O'Hara  "]
SYNC_USER_PSWD_LST = [' QweRty1234', 'QweRty1234 ', 'Qwe Rty123', " Yes! It's me  "]

db_main = db_factory( filename = '#' + MAIN_DB_ALIAS, do_not_create = True, do_not_drop = True)
db_repl = db_factory( filename = '#' + REPL_DB_ALIAS, do_not_create = True, do_not_drop = True)

substitutions = [ ('[\t ]+', ' '), ]
act_db_main = python_act('db_main', substitutions=substitutions)
act_db_repl = python_act('db_repl', substitutions=substitutions)

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

def set_sync_replica_user_pswd(db_main_full_path, sync_repl_user_file, sync_repl_pswd_file, u_name, u_pswd):
    err_change_sync_auth_data = ''
    try:
        with open(Path(db_main_full_path).parent / sync_repl_user_file, 'w') as f:
            f.write(u_name)
        with open(Path(db_main_full_path).parent / sync_repl_pswd_file, 'w') as f:
            f.write(u_pswd)
    except OSError as e:
        err_change_sync_auth_data = e.__str__()

    return err_change_sync_auth_data

#--------------------------------------------

@pytest.mark.replication
@pytest.mark.version('>=6.0')
def test_1(act_db_main: Action,  act_db_repl: Action, capsys):

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

    idx = 0
    expected_on_replica = []
    if len(run_errors_map) == 0:
        try:
            for u_name in SYNC_USER_NAME_LST:
                for u_pswd in SYNC_USER_PSWD_LST:
                    idx += 1
                    # write default values in SYNC_REPL_USER_FILE and SYNC_REPL_PSWD_FILE (to have ability replicate role 'service_team' and grants for it):
                    out_set_sync = set_sync_replica_user_pswd(db_info[act_db_main,'db_full_path'], SYNC_REPL_USER_FILE, SYNC_REPL_PSWD_FILE, act_db_main.db.user, act_db_main.db.password)

                    u_name_sql = u_name.replace("'", "''")
                    u_pswd_sql = u_pswd.replace("'", "''")
                    repl_user_preparing_sql = f"""
                        create or alter user "{u_name}" password '{u_pswd_sql}' revoke admin role
                        ^
                        execute block as
                        begin
                            for
                                select rdb$role_name as service_role
                                from rdb$roles
                                where rdb$role_name starting with upper('service_team_')
                                as cursor c
                            do
                                begin
                                    execute statement 'drop role ' || trim(c.service_role);
                                end
                        end
                        ^
                        create role service_team_{idx} set system privileges to IGNORE_DB_TRIGGERS, REPLICATE_INTO_DATABASE
                        ^
                        grant default service_team_{idx} to user "{u_name}"
                        ^
                    """
                    with act_db_main.db.connect() as con:
                        for s in repl_user_preparing_sql.split('^'):
                            if (x := s.strip()):
                                if x.lower() == 'commit':
                                    con.commit()
                                else:
                                    con.execute_immediate(x)
                        con.commit()

                    #--------------------------------------------------
                    # change name and password for user who will does sync replication:
                    out_set_sync = set_sync_replica_user_pswd(db_info[act_db_main,'db_full_path'], SYNC_REPL_USER_FILE, SYNC_REPL_PSWD_FILE, u_name, u_pswd)
                    #--------------------------------------------------

                    repl_user_check_sql = f"""
                        recreate table test(id int primary key, who_name varchar(63), who_pswd varchar(63))
                        ^
                        commit
                        ^
                        insert into test values({idx}, '{u_name_sql}', '{u_pswd_sql}')
                        ^
                    """

                    # this must be applied to replica by <u_name> using password <u_pswd>:
                    with act_db_main.db.connect() as con:
                        for s in repl_user_check_sql.split('^'):
                            if (x := s.strip()):
                                if x.lower() == 'commit':
                                    con.commit()
                                else:
                                    con.execute_immediate(x)
                        con.commit()
                        con.execute_immediate(f'drop user "{u_name}"')

                    expected_on_replica.append( ' '.join( ( 'ID', ':', str(idx) ) ) )
                    expected_on_replica.append( ' '.join( ( 'WHO_NAME', ':', '>' + u_name + '<' ) ) )
                    expected_on_replica.append( ' '.join( ( 'WHO_PSWD', ':', '>' + u_pswd + '<' ) ) )

                    with act_db_repl.db.connect() as con:
                        cur = con.cursor()
                        cur.execute('select * from test')
                        ccol=cur.description
                        for r in cur:
                            for i in range(0,len(ccol)):
                                print( ccol[i][0].ljust(32), ':', r[i] if ccol[i][0] == 'ID'.upper() else f'>{r[i]}<' )
                #< for u_pswd in SYNC_USER_PSWD_LST
            #< for u_name in SYNC_USER_NAME_LST

            # write 'SYSDBA' / 'masterkey' values in SYNC_REPL_USER_FILE and SYNC_REPL_PSWD_FILE:
            out_set_sync = set_sync_replica_user_pswd(db_info[act_db_main,'db_full_path'], SYNC_REPL_USER_FILE, SYNC_REPL_PSWD_FILE, act_db_main.db.user, act_db_main.db.password)

        except DatabaseError as e:
            run_errors_map['main_code_db_err'] = '\n'.join( (f'Content of sync-replica username_file: >' + u_name + '<, password_file: >' + u_pswd + '<', e.__str__(), 'gdscodes:', f'{e.gds_codes}') )
        except Exception as e:
            run_errors_map['main_code_common_err'] = f"Unexpected error during main code execution: {e}"

    # len(run_errors_map) == 0

    log_after = act_db_main.get_firebird_log()

    # Get error messages from firebird.log (if they did appear):
    # ------------------------------------
    p_fb_log = re.compile('(user name and password are not defined)|(error occurred during login)', re.IGNORECASE)
    fb_log_diff = '\n'.join( [line.rstrip() for line in unified_diff(log_before, log_after) if line.startswith('+') and line.split('+') and p_fb_log.search(line) ] )
    run_errors_map['fb_log_diff'] = fb_log_diff

    # Get error messages from replication.log (if they did appear):
    # ------------------------------------
    diff_data = unified_diff(
        replold_lines,
        get_repl_log(act_db_main)
      )

    # Post-check: verify that diff between old and current replication.log does NOT contain 'WARNING:' or 'ERROR:' lines:
    repl_log_diff = '\n'.join( [ line.rstrip() for line in diff_data if 'WARNING:' in line or 'ERROR:' in line ] )
    run_errors_map['repl_log_diff'] = repl_log_diff

    if max(v.strip() for v in run_errors_map.values()):
        run_errors_map['final_reset'] = reset_sync_replication(act_db_main, act_db_repl, db_info[act_db_main,'db_full_path'], db_info[act_db_repl,'db_full_path'])
    else:
        act_db_main.expected_stdout = '\n'.join( expected_on_replica )
        act_db_main.stdout = capsys.readouterr().out
        assert act_db_main.clean_stdout == act_db_main.clean_expected_stdout

    if max(v.strip() for v in run_errors_map.values()):
        print(f'Problem(s) detected, check run_errors_map:')
        for k,v in run_errors_map.items():
            if v.strip():
                print(k,':')
                print(v.strip())
                print('-' * 40)
 
    assert '' == capsys.readouterr().out
