#coding:utf-8

"""
ID:          syspriv.access-shutdown-database
TITLE:       Check ability to access to database in shutdown single mode as non-sysdba
DESCRIPTION:
  We create role with granting system privilege ACCESS_SHUTDOWN_DATABASE to it.
  Then we create user and make this role as DEFAULT to him.
  Then we check that user 'TMP_SYSPRIV_USER':
      1. can NOT CHANGE database attribute, i.e. can NOT shutdown or bring online database;
      2. CAN make attachment to DB in 'shutdown single maintenace' mode and make some DML there.
  Also, we check that while 'TMP_SYSPRIV_USER' is connected, NO other attachment is possible.
  This is done by trying to make ES EDS as SYSDBA - this should fail with "335544528 : database shutdown".

  Checked on 4.0.0.267. See also letter from Alex 23.06.2016 11:46.
FBTEST:      functional.syspriv.access_shutdown_database

NOTES: checked on 4.0.1.2692, 5.0.0.489.
"""
import os
import locale
import random
import string
import pytest
from firebird.qa import *
from firebird.driver import ShutdownMode,ShutdownMethod,SrvStatFlag,DatabaseError
#from firebird.driver.types import DatabaseError

for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

db = db_factory( filename = ''.join(random.choices(string.ascii_lowercase + string.digits, k=8)) + '.fdb' )

tmp_user = user_factory('db', name='tmp_syspriv_user', password='123')
tmp_role = role_factory('db', name='tmp_role_for_access_shutdown_db')

act = python_act('db')

@pytest.mark.es_eds
@pytest.mark.version('>=4.0')

#--------------------------------------------------------------------

def show_db_info(act: Action): # , tmp_user: User, tmp_role: Role):

    retdata = ['Data from DB header:']
    with act.connect_server(user = act.db.user, password = act.db.password) as srv:
        srv.database.get_statistics(database = act.db.db_path, flags = SrvStatFlag.HDR_PAGES)
        stat_output = [x.rstrip() for x in srv.readlines() if x.strip()]
        for i,line in enumerate(stat_output):
            if 'database' in line.lower() or 'attributes' in line.lower():
                retdata.append(line)

    retdata.append( 'Data from mon$database:' )
    try:
        #with act.db.connect(user = tmp_user.name, password = tmp_user.password) as con:
        with act.db.connect(user = act.db.user, password = act.db.password) as con:
            cur = con.cursor()
            #cur.execute('select current_timestamp,current_user,mon$database_name,mon$shutdown_mode,mon$read_only,mon$creation_date,mon$owner,mon$sec_database from mon$database')
            cur.execute('select current_timestamp,v.* from v_check as v')
            hdr=cur.description
            for r in cur:
                for i in range(len(hdr)):
                    retdata.append( hdr[i][0].ljust(32) + ':' + f'{r[i]}' )
    except DatabaseError as e:
        retdata.append( e.__str__() )

    return '\n'.join( retdata )

#--------------------------------------------------------------------

def test_1(act: Action, tmp_user: User, tmp_role:Role, capsys):

    # Map for storing mnemonas and details for every FAILED step:
    run_errors_map = {}

    init_script = \
    f'''
        set wng off;
        alter user {tmp_user.name} revoke admin role;
        revoke all on all from {tmp_user.name};

        alter role {tmp_role.name}
            set system privileges to ACCESS_SHUTDOWN_DATABASE; -- CHANGE_SHUTDOWN_MODE, USE_GFIX_UTILITY, IGNORE_DB_TRIGGERS;
        commit;
        grant default {tmp_role.name} to user {tmp_user.name};
        commit;
    '''
    act.isql(switches=['-q'], input=init_script, combine_output = True)
    if act.clean_stdout:
        run_errors_map['init_err'] = 'Init script failed.\n' + act.clean_stdout
    act.reset()

    # ---------------------------------------------------------------

    if len(run_errors_map) == 0:
        # Must FAIL: tmp_user has right only to *access* to DB in shutdown-single mode and make some DMLs there.
        # But he has NO right to change DB state to shutdown (any kind of mode).
        # Expected error: "no permission for shutdown/online access to database ..."
        
        act.gfix( switches = ['-user', tmp_user.name, '-pas', tmp_user.password, '-shutdown', 'single', '-force', '0', act.db.dsn], credentials = False, combine_output = True, io_enc = locale.getpreferredencoding() )

        # !!!UNSTABLE RESULT!!! 28.01.2026
        # act.svcmgr( switches = ['localhost:service_mgr', 'user', tmp_user.name, 'password', tmp_user.password, 'action_properties', 'dbname', act.db.db_path, 'prp_shutdown_mode', 'prp_sm_single', 'prp_force_shutdown', '0'], connect_mngr = False, io_enc = locale.getpreferredencoding() )
        # act.connect_server(user = tmp_user.name, password = tmp_user.password) as srv_nondba

        msg_prefix = f'CAUTION. Change DB state to ShutdownMode.SINGLE running by {tmp_user.name} UNEXPECTEDLY'
        if act.clean_stdout and act.return_code != 0:
            pass
        elif act.return_code == 0:
            run_errors_map['shut_single_err0'] = f'{msg_prefix} returned {act.return_code=}.\n' + show_db_info(act)
        else:
            run_errors_map['shut_single_err1'] = f'{msg_prefix} passed.\n' + show_db_info(act)

        act.reset()

    #-----------------------------------------------------------------

    if len(run_errors_map) == 0:
        # Must PASS: we change DB state to shut-single using SYSDBA account.
        # No message must be issued now:
        act.gfix( switches = ['-user', act.db.user, '-pas', act.db.password, '-shutdown', 'single', '-force', '0', act.db.dsn], credentials = False, combine_output = True, io_enc = locale.getpreferredencoding() )

        '''
        with act.connect_server() as srv_sysdba:
            try:
                srv_sysdba.database.shutdown(database=act.db.db_path
                                      ,mode=ShutdownMode.SINGLE
                                      ,method=ShutdownMethod.FORCED
                                      ,timeout=0)
            except DatabaseError as e:
                run_errors_map['shut_single_err2'] = f'### CAUTION-1 ### shutdown(ShutdownMode.SINGLE) running by {act.db.user} UNEXPECTEDLY failed.\n' + e.__str__() + '\n' + show_db_info(act)
                #print(e.__str__())
        '''
        #act.expected_stdout = ''
        #act.stdout = capsys.readouterr().out
        #assert act.clean_stdout == act.clean_expected_stdout # <<<<<<<<<<<<<<<<<<<<<<<< check #1

        msg_prefix = f'CAUTION. Change DB state to ShutdownMode.SINGLE running by {act.db.user} UNEXPECTEDLY'
        if act.clean_stdout:
            run_errors_map['shut_single_err2'] = f'{msg_prefix} failed.\n' + act.clean_stdout + '\n' + show_db_info(act)
        elif act.return_code != 0:
            run_errors_map['shut_single_err3'] = f'{msg_prefix} returned {act.return_code}.\n' + act.clean_stdout + '\n' + show_db_info(act)
        else:
            pass   
        act.reset()

    # ---------------------------------------------------------------
    if len(run_errors_map) == 0:
        # Result: DB now is in shutdown-single mode.
        # We have to check that only single attachment can be established to this DB:

        sql_chk = f'''
            set list on;
            set term ^;
            execute block returns( who_else_here rdb$user ) as
                declare another_user varchar(31);
            begin
                execute statement 'select current_user from rdb$database'
                on external 'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
                as user '{act.db.user}' password '{act.db.password}'
                into who_else_here;

                suspend;
            end
            ^
            set term ;^
        '''
        act.isql(switches=['-q', '-user', tmp_user.name, '-pas', tmp_user.password], input=sql_chk, credentials = False, combine_output=True)

        msg_prefix = f'CAUTION. SQL for check that only one attachment is allowed'
        if 'SQLSTATE = 42000' in act.clean_stdout:
            pass
        elif act.return_code == 0:
            run_errors_map['check_single_att_allowed1'] = f'{msg_prefix} UNEXPECTEDLY returned {act.return_code}'
        else:
            run_errors_map['check_single_att_allowed2'] = f"{msg_prefix} UNEXPECTEDLY has no required 'SQLSTATE' code:\n{act.clean_stdout}"
        act.reset()

    # ---------------------------------------------------------------

    if len(run_errors_map) == 0:
        # must FAIL: we attempt to bring DB online using NON-dba account:
        # Expected error: "no permission for bring online access to database ..."
        # !!!NB!!! As of 25.09.2022, for FB 4.x and 5.x in Classic mode additional message will raise here:
        # "-Some database(s) were shutdown when trying to read mapping data"
        # Sent report to Alex et al, 25.09.2022 18:55. Waiting for resolution.
        #
        '''
        !!! UNSTABLE RESULT !!! 28.01.2026
        with act.connect_server(user = tmp_user.name, password = tmp_user.password) as srv_nondba:
        '''

        act.gfix( switches = ['-user', tmp_user.name, '-pas', tmp_user.password, '-online', act.db.dsn], credentials = False, combine_output = True, io_enc = locale.getpreferredencoding() )
        
        msg_prefix = f'CAUTION. Change DB state to ONLINE running by {tmp_user.name} UNEXPECTEDLY'
        if act.clean_stdout and act.return_code != 0:
            pass
        elif act.return_code == 0:
            run_errors_map['bring_online_err0'] = f'{msg_prefix} returned {act.return_code=}.\n' + show_db_info(act)
        else:
            run_errors_map['bring_online_err1'] = f'{msg_prefix} passed.\n' + show_db_info(act)

        act.reset()

    if len(run_errors_map) == 0:
        # must PASS because here we return DB online using SYSDBA account:
        '''
        !!! UNSTABLE RESULT !!! 28.01.2026
        with act.connect_server() as srv_sysdba:
            try:
                srv_sysdba.database.bring_online(database=act.db.db_path)
            except DatabaseError as e:
                run_errors_map['bring_online_err2'] = f'### CAUTION-4 ### bring_online() running by {act.db.user} UNEXPECTEDLY failed.\n' + e.__str__() + '\n' + show_db_info(act)
        #assert '' == capsys.readouterr().out
        #assert act.clean_stdout == act.clean_expected_stdout # <<<<<<<<<<<<<<<<<<<<<<<< check #4
        '''

        act.gfix( switches = ['-user', act.db.user, '-pas', act.db.password, '-online', act.db.dsn], credentials = False, combine_output = True, io_enc = locale.getpreferredencoding() )
        
        msg_prefix = 'CAUTION. Call bring_online() performed by {act.db.user} UNEXPECTEDLY'

        if not act.clean_stdout and act.return_code == 0:
            pass
        elif act.return_code != 0:
            run_errors_map['bring_online_err2'] = f'{msg_prefix} returned {act.return_code=}.\n' + show_db_info(act)
        else:
            run_errors_map['bring_online_err3'] = f'{msg_prefix} failed.\n' + act.clean_stdout + show_db_info(act)

        act.reset()


    if run_errors_map and max(v.strip() for v in run_errors_map.values()):
        print(f'Problem(s) detected, check run_errors_map:')
        for k,v in run_errors_map.items():
            if v.strip():
                print(k,':')
                print(v.strip())
                print('-' * 40)
 
    assert '' == capsys.readouterr().out
