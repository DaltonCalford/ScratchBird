#coding:utf-8

"""
ID:          syspriv.change-shutdown-mode
TITLE:       Check ability to change database shutdown mode by non-sysdba user who is
  granted with necessary system privileges
DESCRIPTION:
    Test creates common user and role, then it grants system privileged to that role
    and, in turn, grants role to user. Because role is granted as DEFAULT, we can connect
    to services without specifying it.
    Further, we change DB state to shutdown and bring it online several times, using this
    non-DBA user account. All these actions must not raise exception.
    NB: role must be granted with 'IGNORE_DB_TRIGGERS' privilege in order to bypass DB-level
    triggers when user is attaching to DB which has such triggers. Test verifies this by adding
    table which is filled by DB-level trigger on connect. This table must remain EMPTY at the
    final point of the test, see 'SELECT COUNT(*) FROM ATT_LOG' query.

FBTEST:      functional.syspriv.change_shutdown_mode
NOTES:
    [20.05.2022] pzotov
    In order to verify ability to change DB state to SINGLE or MULTI, one need to grant system privilege
    ACCESS_SHUTDOWN_DATABASE, oterwise attempt to change DB state to such mode will fail with error:
    "database ... shutdown".
    Explanation: https://github.com/FirebirdSQL/firebird/issues/7189#issuecomment-1132731509

    Checked on 4.0.1.2692, 5.0.0.497.
"""

import pytest
from firebird.qa import *
from firebird.driver import ShutdownMode,ShutdownMethod
from firebird.driver.types import DatabaseError

substitutions = [('[ \t]+', ' ')]
db = db_factory()
tmp_user = user_factory('db', name='tmp_syspriv_user', password='123')
tmp_role = role_factory('db', name='tmp_role_for_chng_shutdown_mode')

act = python_act('db', substitutions = substitutions)

expected_stdout_isql = "ATT_LOG_COUNT 0"


@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user: User, tmp_role:Role, capsys):

    #-----------------------------------------------
    def bring_tmp_db_online( srv, db_path ):
        srv.database.bring_online(database=db_path)
    #-----------------------------------------------

    init_script = \
    f'''
        set wng off;
        set bail on;
        set list on;
        set count on;

        create or alter view v_check as
        select
             current_user as who_ami
            ,r.rdb$role_name
            ,rdb$role_in_use(r.rdb$role_name) as RDB_ROLE_IN_USE
            ,r.rdb$system_privileges
        from mon$database m cross join rdb$roles r;
        commit;

        alter user {tmp_user.name} password '123' revoke admin role;
        revoke all on all from {tmp_user.name};

        create or alter trigger trg_connect active on connect as
        begin
        end;
        commit;

        recreate table att_log (
            att_id int,
            att_name varchar(255),
            att_user varchar(255),
            att_addr varchar(255),
            att_prot varchar(255),
            att_dts timestamp default 'now'
        );

        commit;

        grant select on v_check to public;
        grant all on att_log to public;
        commit;

        set term ^;
        create or alter trigger trg_connect active on connect as
        begin
          if ( upper(current_user) <> upper('SYSDBA') ) then
             in autonomous transaction do
             insert into att_log(att_id, att_name, att_user, att_prot)
             select
                  mon$attachment_id
                 ,mon$attachment_name
                 ,mon$user
                 ,mon$remote_protocol
             from mon$attachments
             where mon$user = current_user
             ;
        end
        ^
        set term ;^
        commit;


        -- NB: Privilege 'IGNORE_DB_TRIGGERS' is needed when we return database to ONLINE
        -- and this DB has DB-level trigger.
        alter role {tmp_role.name}
            set system privileges to CHANGE_SHUTDOWN_MODE, USE_GFIX_UTILITY, IGNORE_DB_TRIGGERS, ACCESS_SHUTDOWN_DATABASE;
        commit;
        grant default {tmp_role.name} to user {tmp_user.name};
        commit;
    '''
    act.isql(switches=['-q'], input=init_script)

    with act.connect_server(user = tmp_user.name, password = tmp_user.password, role = tmp_role.name) as srv_nondba:
        # All subsequent actions must not issue any output:
        try:
            for checked_shut_mode in (ShutdownMode.FULL, ShutdownMode.MULTI, ShutdownMode.SINGLE):
                for checked_shut_method in (ShutdownMethod.FORCED, ShutdownMethod.DENY_ATTACHMENTS, ShutdownMethod.DENY_TRANSACTIONS):
                    for checked_timeout in (0,1):
                        srv_nondba.database.shutdown(database=act.db.db_path
                                              ,mode=checked_shut_mode
                                              ,method=checked_shut_method
                                              ,timeout=checked_timeout)
                        bring_tmp_db_online(srv_nondba, act.db.db_path)

        except DatabaseError as e:
            print(e.__str__())


    sql_check = "set list on; select count(*) as att_log_count from att_log;"
    act.isql(switches=['-q'], input=sql_check)
    act.expected_stdout = expected_stdout_isql
    assert act.clean_stdout == act.clean_expected_stdout

