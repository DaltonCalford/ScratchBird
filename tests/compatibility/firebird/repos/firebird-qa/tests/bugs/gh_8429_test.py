#coding:utf-8

"""
ID:          issue-8429
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8429
TITLE:       Segfault when already destroyed callback interface is used
DESCRIPTION:
    Test uses THREE preliminary created aliases with KeyHolderPlugin that points to special configuration with name 'KH2'.
    This configuration makes crypt plugin accept key ONLY from client app (in contrary to other encryption-related tests).
    Three non-privileged users are created for this test.
NOTES:
    [24.02.2025] pzotov
    1) firebird.conf must have KeyHolderPlugin = fbSampleKeyHolder and plugins/fbSampleKeyHolder.conf must have Auto = true.
    2) aliases gh_8429_alias_a, gh_8429_alias_b, gh_8429_alias_c that are used in this test must have (in databases.conf)
       KeyHolderPlugin = KH2 and plugins.conf must contain:
       ---------------
       Plugin = KH2 {
           Module = $(dir_plugins)/fbSampleKeyHolder
           RegisterName = fbSampleKeyHolder
           Config = KH2
       }
       Config = KH2 {
           Auto = false
       }
       ---------------
    
    3) Third database (defined by 'gh_8429_alias_c') must be encrypted in FB-6.x in order to reproduce bug.
    4) All databases are created here by explicit 'create_database()' call and their deletion does NOT perform.
       This was done intentionally in order to suppress appearance of any encryption-related errors that do not matter.
    5) Method 'create_database()' uses LOCAL protocol if $QA_ROOT/firebir-driver.conf has no section [firebird.server.defaults]
       It can be empty but it MUST present if we want database to be created using prefix 'inet://' for its DSN.
       Because of that, encryption of db_file_c will be done at exclusive connection, see reduced indent for 'run_encr_decr()' call.

    Great thanks to Alex for provided scenario for this test (letter 10-feb-2025 20:43) and lot of suggestions.

    Confirmed bug only for Super, snapshots: 6.0.0.601-6af07d0; 5.0.2.1606-a92f352; 4.0.6.3181-00b648f.
    Classic not affected.
    Checked on 6.0.0.607-1985b88; 5.0.2.1610-5e63ad0; 4.0.6.3185-9cac45a - all fine.
"""
import os
import time
import datetime as py_dt
import re
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import create_database, ShutdownMode, ShutdownMethod, DatabaseError


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

db = db_factory()
usr_x = user_factory('db', name = 'tmp_8429_x', password = 'tmp_8429_x', plugin = 'Srp')
usr_y = user_factory('db', name = 'tmp_8429_y', password = 'tmp_8429_y', plugin = 'Srp')
usr_z = user_factory('db', name = 'tmp_8429_z', password = 'tmp_8429_z', plugin = 'Srp')

db_a = db_factory(filename = '#gh_8429_alias_a', do_not_create = True, do_not_drop = True)
db_b = db_factory(filename = '#gh_8429_alias_b', do_not_create = True, do_not_drop = True)
db_c = db_factory(filename = '#gh_8429_alias_c', do_not_create = True, do_not_drop = True)

act_a = python_act('db_a')
act_b = python_act('db_b')
act_c = python_act('db_c')

#----------------------------------------------------

def get_filename_by_alias(act: Action, alias_from_dbconf = None):
    
    if not alias_from_dbconf:
        alias_from_dbconf = act.db.db_path

    # Scan line-by-line through databases.conf, find line starting with <alias_from_dbconf> and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + alias_from_dbconf + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_6147_alias = $(dir_sampleDb)/qa/tmp_core_6147.fdb
                # - then we extract filename: 'tmp_core_6147.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably <alias_from_dbconf> not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    #------------------------------------------------------------------
    # Full path + filename of database to which we will try to connect:
    #
    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )

    return tmp_fdb

#-----------------------------------------------------------------------

def run_encr_decr(act: Action, mode, max_wait_encr_thread_finish, capsys):
    if mode == 'encrypt':
        # See letter from Alex, 15.12.2023 16:16 demo-plugin can not transfer named key over network.
        # Because of that, we have to use 'ALTER DATABASE ENCRYPT WITH <plugin>'  _WITHOUT_ adding 'key "{ENCRYPTION_KEY}"'.
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
        alter_db_sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}"' # <<< ::: NB ::: DO NOT add '... key "{ENCRYPTION_KEY}"' here!
        wait_for_state = 'Database encrypted'
    elif mode == 'decrypt':
        alter_db_sttm = 'alter database decrypt'
        wait_for_state = 'Database not encrypted'


    e_thread_finished = False

    # 0 = non crypted;
    # 1 = has been encrypted;
    # 2 = is DEcrypting;
    # 3 = is Encrypting;
    #
    REQUIRED_CRYPT_STATE = 1 if mode == 'encrypt' else 0
    current_crypt_state = -1
    d1 = py_dt.timedelta(0)
    with act.db.connect() as con:
        cur = con.cursor()
        ps, rs = None, None
        try:
            ps = cur.prepare('select mon$crypt_state from mon$database')
            t1=py_dt.datetime.now()
            d1 = t1-t1
            con.execute_immediate(alter_db_sttm)
            con.commit()
            while True:
                t2=py_dt.datetime.now()
                d1=t2-t1
                if d1.seconds*1000 + d1.microseconds//1000 > max_wait_encr_thread_finish:
                    break
    
                # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                # We have to store result of cur.execute(<psInstance>) in order to
                # close it explicitly.
                # Otherwise AV can occur during Python garbage collection and this
                # causes pytest to hang on its final point.
                # Explained by hvlad, email 26.10.24 17:42
                rs = cur.execute(ps)
                for r in rs:
                    ######################################################
                    ###   C H E C K    M O N $ C R Y P T _ S T A T E   ###
                    ######################################################
                    current_crypt_state = r[0]
                con.commit()
                if current_crypt_state == REQUIRED_CRYPT_STATE:
                    e_thread_finished = True
                    break
                else:
                    time.sleep(0.5)
        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if rs:
                rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
            if ps:
                ps.free()


    assert e_thread_finished, f'TIMEOUT EXPIRATION. Mode="{mode}" took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {max_wait_encr_thread_finish} ms; current_crypt_state={current_crypt_state}'


#-----------------------------------------------------------------------

@pytest.mark.es_eds
@pytest.mark.encryption
@pytest.mark.version('>=4.0.6')
def test_1(act_a: Action, act_b: Action, act_c: Action, usr_x: User, usr_y: User, usr_z: User, capsys):

    # ::: NB :::
    # Encryption required for db_file_c in order to reproduce problem on FB 6.x
    # Because of that, this DB can be inaccessibe something was wrong in previous run of this test.
    # Thus we have to create all databases now (rather than use previously created):
    #
    dbfile_a = get_filename_by_alias(act_a)
    dbfile_b = get_filename_by_alias(act_b)
    dbfile_c = get_filename_by_alias(act_c)

    dbfile_a.unlink(missing_ok = True)
    dbfile_b.unlink(missing_ok = True)
    dbfile_c.unlink(missing_ok = True)

    # xxx old comment xxx
    # 'create_database()' will use LOCAL protocol if $QA_ROOT/firebir-driver.conf has no section [firebird.server.defaults]
    # It can be empty but it MUST present if we want database to be created using prefix 'inet://' for its DSN.
    # xxxxxxxxxxxxxxxxxxx
    # ::: NB :::
    # act_*.db.db_path is an ALIAS, not full path to DB file!
    # firebird-driver function _is_dsn(act_*.db.db_path) will return False for this value because it does not look like 'c:\path\to\'
    # If we call create_database() with specifying such alias without custom instance of db_config AND without protocol ('localhost:', 'inet://' or 'xnet://')
    # then code in this function will not define 'dsn' variable and will fail with:
    #              else:  # pragma: no cover
    #  >               raise InterfaceError("Result code does not match request code")
    #  E               firebird.driver.types.InterfaceError: Result code does not match request code
    # Because of this, we have to explicitly specify protocol here!
    #
    with create_database('inet://' + act_a.db.db_path, user = act_a.db.user, password = act_a.db.password) as con_a, \
        create_database('inet://' + act_b.db.db_path, user = act_b.db.user, password = act_b.db.password) as con_b, \
        create_database('inet://' + act_c.db.db_path, user = act_c.db.user, password = act_c.db.password) as con_c:
        
        sql = f"""        
            create procedure sp_a (a_who varchar(31)) returns (o_info varchar(512)) as
            begin
                -- e1-meta.sql:
                execute statement 'select * from sp_b(''' || a_who || ''')' as user '{usr_x.name}' password '{usr_x.password}' on external 'gh_8429_alias_b' into o_info;
                suspend;
            end
            ^
            grant execute on procedure sp_a to public
            ^
        """
        for x in sql.split('^'):
            if (s := x.strip()):
                con_a.execute_immediate(s)
        con_a.commit()

        # ..................................................

        sql = f"""
            create procedure sp_b (a_who varchar(31)) returns (o_info varchar(512)) as
            begin
                execute statement 'select * from sp_c' as user a_who password a_who on external 'gh_8429_alias_c' into o_info;
                suspend;
            end
            ^
            grant execute on procedure sp_b to public
            ^
        """
        for x in sql.split('^'):
            if (s := x.strip()):
                con_b.execute_immediate(s)
        con_b.commit()

        # ..................................................

        sql = f"""
            create procedure sp_c returns (o_info varchar(512)) as
            begin
                o_info = lower(current_user);
                -- o_info = lower(rdb$get_context('SYSTEM', 'DB_NAME')) || ':' || lower(current_user);
                suspend;
            end
            ^
            grant execute on procedure sp_c to public
            ^
        """
        for x in sql.split('^'):
            if (s := x.strip()):
                con_c.execute_immediate(s)
        con_c.commit()
    # < close all connections
    
    ############################################
    ###   E N C R Y P T    D A T A B A S E   ###
    ############################################
    # Run encryption. No concurrent connection must be here to db_file_c:
    run_encr_decr(act_c, 'encrypt', MAX_WAITING_ENCR_FINISH, capsys)

    with act_a.db.connect() as con_a:
        cur = con_a.cursor()
        cur.execute(f"select o_info from sp_a('{usr_x.name.lower()}')")
        for r in cur:
            print(r[0])

    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    # this caused crash before fix:
    # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    with act_a.db.connect() as con_a:
        cur = con_a.cursor()
        cur.execute(f"select o_info from sp_a('{usr_y.name.lower()}')")
        for r in cur:
            print(r[0])
        
    #########
    # CLEANUP
    #########
    for a in (act_a, act_b, act_c):
        with a.db.connect() as con:
            con.execute_immediate('ALTER EXTERNAL CONNECTIONS POOL CLEAR ALL')
            con.commit()
            con.drop_database()

    act_a.expected_stdout = f"""
        {usr_x.name.lower()}
        {usr_y.name.lower()}
    """
    act_a.stdout = capsys.readouterr().out
    assert act_a.clean_stdout == act_a.clean_expected_stdout
    act_a.reset()
