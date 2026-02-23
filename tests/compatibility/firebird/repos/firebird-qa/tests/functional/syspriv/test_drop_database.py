#coding:utf-8

"""
ID:          syspriv.drop-database
TITLE:       Check ability to DROP database by non-sysdba user who is granted with necessary system privileges
DESCRIPTION:
  We make backup and restore of current DB to other name ('functional.syspriv.drop_database.tmp').
  Than we attach to DB 'functional.syspriv.drop_database.tmp' as user U01 and try to DROP it.
  This should NOT raise any error, database file should be deleted from disk.
FBTEST:      functional.syspriv.drop_database

NOTES:
    [20.05.2022] pzotov
    Test creates TEMPORARY database (beside of 'main' one) and uses SYSDBA for that.
    Then it creates NON-dba user and role with system privilege DROP_DATABASE.
    Finally, it grants role to non-dba user, makes connect to temporary DB and tries to DROP it using NON-dba user.
    Checked on 4.0.1.2692, 5.0.0.497.
"""

import pytest
from firebird.qa import *
from firebird.driver.types import DatabaseError

substitutions = [('[ \\t]+', ' '), ('DB_NAME.*TMP4TEST.TMP', 'DB_NAME TMP4TEST.TMP')]
db_main = db_factory()
tmp_user = user_factory('db_main', name='tmp_syspriv_user', password='123')
tmp_role = role_factory('db_main', name='tmp_role_for_drop_database')
act = python_act('db_main', substitutions = substitutions)

db_temp = db_factory(filename = 'tmp4test.tmp', do_not_create=True, do_not_drop=True)

expected_stdout_isql = """
    DB_NAME                         TMP4TEST.TMP
    WHO_AMI                         TMP_SYSPRIV_USER
    RDB$ROLE_NAME                   RDB$ADMIN
    RDB_ROLE_IN_USE                 <false>
    RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF
    DB_NAME                         TMP4TEST.TMP
    WHO_AMI                         TMP_SYSPRIV_USER
    RDB$ROLE_NAME                   TMP_ROLE_FOR_DROP_DATABASE
    RDB_ROLE_IN_USE                 <true>
    RDB$SYSTEM_PRIVILEGES           0004000000000000
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user: User, tmp_role:Role, db_temp: Database, capsys):
    init_script = \
    f"""
        set wng off;
        set list on;
        set bail on;

        create database '{db_temp.dsn}' user {act.db.user} password '{act.db.password}'; -- DB is created by ### SYSDBA ###

        create or alter view v_check as
        select
            upper(mon$database_name) as db_name
            ,current_user as who_ami
            ,r.rdb$role_name
            ,rdb$role_in_use(r.rdb$role_name) as RDB_ROLE_IN_USE
            ,r.rdb$system_privileges
        from mon$database m cross join rdb$roles r
        order by r.rdb$role_name;
        commit;
        grant select on v_check to public;
        commit;

        alter user {tmp_user.name} revoke admin role;
        revoke all on all from {tmp_user.name};
        commit;
        create role {tmp_role.name} set system privileges to DROP_DATABASE;
        commit;
        grant default {tmp_role.name} to user {tmp_user.name};
        commit;
        connect '{db_temp.dsn}' user {tmp_user.name} password '{tmp_user.password}';
        select * from v_check;
        commit;
        drop database; -- DB is dropped by ### NON-DBA ### who has granted with apropriate role with system privilege.
    """

    act.isql(switches=['-q'], input=init_script, connect_db = False, credentials = False, combine_output=True)
    act.expected_stdout = expected_stdout_isql
    assert act.clean_stdout == act.clean_expected_stdout

