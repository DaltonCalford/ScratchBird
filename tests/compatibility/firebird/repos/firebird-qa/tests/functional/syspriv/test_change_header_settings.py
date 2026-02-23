#coding:utf-8

"""
ID:          syspriv.change-header-settings
TITLE:       Check ability to change some database header attributes by non-sysdba user who is granted with necessary system privileges
DESCRIPTION:
  NB: attributes should be changed one at a time, i.e. one Services API call should change only ONE atribute.
FBTEST:      functional.syspriv.change_header_settings

NOTES:
    [18.05.2022] pzotov: refactored to be used in firebird-qa suite.
    Checked on 4.0.1.2692, 5.0.0.489.
"""

import pytest
from firebird.qa import *
from firebird.driver import DbSpaceReservation
from firebird.driver.types import DatabaseError

substitutions = [('[ \t]+', ' ')]
db = db_factory()
tmp_user = user_factory('db', name='tmp_syspriv_user', password='123')
tmp_role = role_factory('db', name='tmp_role_for_change_db_header')

act = python_act('db', substitutions = substitutions)

expected_stdout_isql = """
    MON$SWEEP_INTERVAL              54321
    MON$SQL_DIALECT                 1
    MON$RESERVE_SPACE               0
"""


@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user: User, tmp_role:Role, capsys):
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
            set system privileges to CHANGE_HEADER_SETTINGS, USE_GFIX_UTILITY, IGNORE_DB_TRIGGERS;
        commit;
        grant default {tmp_role.name} to user {tmp_user.name};
        commit;
    '''
    act.isql(switches=['-q'], input=init_script)

    with act.connect_server(user = tmp_user.name, password = tmp_user.password, role = tmp_role.name) as srv_nondba:
        # All subsequent actions must not issue any output:
        try:
            # change SWEEP interval:
            srv_nondba.database.set_sweep_interval(database=act.db.db_path, interval = 54321)

            # set SQL dialect to 1:
            srv_nondba.database.set_sql_dialect(database=act.db.db_path, dialect = 1)

            # change default DB cache size:
            srv_nondba.database.set_space_reservation(database=act.db.db_path, mode = DbSpaceReservation.USE_FULL)

        except DatabaseError as e:
            print(e.__str__())

    sql_check = "set list on; select m.mon$sweep_interval, m.mon$sql_dialect, m.mon$reserve_space from mon$database m;"

    act.isql(switches=['-q'], input=sql_check)

    act.expected_stdout = expected_stdout_isql
    assert act.clean_stdout == act.clean_expected_stdout
