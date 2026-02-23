#coding:utf-8

"""
ID:          syspriv.use-gbak-utility
TITLE:       Check ability to to make database backup
DESCRIPTION:
FBTEST:      functional.syspriv.use_gbak_utility
NOTES:
   [23.05.2022] pzotov
   Checked on 4.0.1.2692, 5.0.0.497.
"""

import pytest
from pathlib import Path
from firebird.qa import *
from firebird.driver.types import DatabaseError, SrvBackupFlag, SrvRestoreFlag

substitutions = [('[ \t]+', ' '), ('ATT_NAME .*TEST.FDB', 'ATT_NAME TEST.FDB'), ('TCPv(4|6){1}', 'TCP')]

db = db_factory()

db_temp = db_factory(filename = 'tmp_use_gbak.fdb.tmp')
fbk_tmp1 = temp_file('tmp_backup_via_services_api.fbk.tmp')
fbk_tmp2 = temp_file('tmp_backup_via_gbak_utility.fbk.tmp')
fdb_temp = temp_file('tmp_restored.fdb.tmp')

tmp_user = user_factory('db', name='tmp_syspriv_user', password='123')
tmp_role = role_factory('db', name='tmp_role_for_use_gbak_utility')

act = python_act('db', substitutions=substitutions)

expected_stdout_gbak = """
    Use Services API. Backup as NON-DBA user completed OK.
    Use gbak utility. Backup as NON-DBA user completed OK.

"""

expected_stdout_isql="""
    ATT_NAME TEST.FDB
    ATT_USER TMP_SYSPRIV_USER
    ATT_ROLE TMP_ROLE_FOR_USE_GBAK_UTILITY
    ATT_PROT TCP
    SYS_PRIV_MASK 0008010000000000

    ATT_NAME TEST.FDB
    ATT_USER TMP_SYSPRIV_USER
    ATT_ROLE TMP_ROLE_FOR_USE_GBAK_UTILITY
    ATT_PROT TCP
    SYS_PRIV_MASK 0008010000000000

    Records affected: 2
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user: User, tmp_role:Role, fbk_tmp1: Path, fbk_tmp2: Path, fdb_temp: Path, capsys):

    init_script = f"""
        set wng off;
        set bail on;
        set list on;
        set count on;

        create or alter view v_check as
        select
            upper(mon$database_name) as db_name
            ,current_user as who_ami
            ,r.rdb$role_name as sys_role_name
            ,rdb$role_in_use(r.rdb$role_name) as rdb_role_in_use
            ,r.rdb$system_privileges as sys_priv_mask
        from mon$database m cross join rdb$roles r;
        commit;

        alter user {tmp_user.name} password '123' revoke admin role;
        revoke all on all from {tmp_user.name};
        commit;

        recreate table test(x int, b blob);
        commit;

        insert into test values(1, upper('qwertyuioplkjhgfdsazxcvbnm') );
        commit;

        recreate table att_log (
            att_id int,
            att_name varchar(255),
            att_user varchar(255),
            att_role varchar(255),
            att_prot varchar(255),
            sys_priv_mask rdb$system_privileges -- binary(8) nullable
        );

        commit;

        grant select on v_check to public;
        grant all on att_log to public;
        -- [ !! ] do NOT: grant select on test to {tmp_user.name}; -- [ !! ]
        commit;

        set term ^;
        create or alter trigger trg_connect active on connect as
        begin
          if ( upper(current_user) <> upper('SYSDBA') ) then
             in autonomous transaction do
             insert into att_log(att_id, att_name, att_user, att_role, att_prot, sys_priv_mask)
             select
                  a.mon$attachment_id
                 ,upper(a.mon$attachment_name)
                 ,a.mon$user
                 ,v.sys_role_name
                 ,a.mon$remote_protocol
                 ,v.sys_priv_mask
             from mon$attachments a
             join v_check v on v.rdb_role_in_use is true
             where a.mon$user = current_user
             ;

        end
        ^
        set term ;^
        commit;

        -- Ability to make database backup.
        -- NB: SELECT_ANY_OBJECT_IN_DATABASE - mandatory for reading data from tables et al.
        alter role {tmp_role.name}
            set system privileges to USE_GBAK_UTILITY, SELECT_ANY_OBJECT_IN_DATABASE;
        commit;
        grant default {tmp_role.name} to user {tmp_user.name};
        commit;

    """
    act.isql(switches=['-q'], input=init_script)

    with act.connect_server(user = tmp_user.name, password = tmp_user.password) as srv_nondba:
        try:
            srv_nondba.database.backup( database=act.db.db_path, backup=fbk_tmp1, callback=act.print_callback )
            print('Use Services API. Backup as NON-DBA user completed OK.')
        except DatabaseError as e:
            print(e.__str__())

    try:
        act.gbak(switches=['-b', '-se', 'localhost:service_mgr', '-user', tmp_user.name, '-password', tmp_user.password, act.db.db_path, fbk_tmp2], combine_output = True )
        print('Use gbak utility. Backup as NON-DBA user completed OK.')
    except DatabaseError as e:
        print(e.__str__())

    act.stdout = capsys.readouterr().out
    act.expected_stdout = expected_stdout_gbak
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # Final check: we must be sure that backup actually was done by NON-dba user rather than 'silently' by SYSDBA:
    act.isql(switches=['-q'], input='set list on; set count on; select att_name, att_user, att_role, att_prot, sys_priv_mask from att_log order by att_id;', connect_db = True, combine_output=True)
    act.expected_stdout = expected_stdout_isql
    assert act.clean_stdout == act.clean_expected_stdout
