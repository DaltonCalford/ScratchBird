#coding:utf-8

"""
ID:          syspriv.grant-revoke-any-object
TITLE:       Check ability to query, modify and deleting data plus add/drop constraints on any table
DESCRIPTION:
  Two users are created, U01 and U02.
  User U01 is granted with system privilege grant_revoke_any_object.
  User U02 has NO any privilege.
  User U01 then creates table and issue GRANT SELECT statement for U02 (WITHOUT using 'granted by clause).
  Then we
  1) check result (contrent of RDB$ tables)
  2) connect as U02 and query this table - this should work OK
  3) connect as U01 and revoke grant on just queried table from U02
  4) connect again as U02 and repeat select - this shoiuld fail.
FBTEST:      functional.syspriv.grant_revoke_any_object
"""

import pytest
from firebird.qa import *

db = db_factory()
user_01 = user_factory('db', name='u01', password = '123')
user_02 = user_factory('db', name='u02', password = '456')
role_revoke = role_factory('db', name='role_for_grant_revoke_any_object')

act = isql_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action, user_01: User, user_02: User, role_revoke: Role):

    test_script = f"""
        set wng off;
        set bail on;
        set list on;

        alter user {user_01.name} revoke admin role;
        alter user {user_02.name} revoke admin role;
        grant create table to {user_01.name};
        commit;

        -- Add/change/delete non-system records in RDB$TYPES
        alter role {role_revoke.name} set system privileges to GRANT_REVOKE_ON_ANY_OBJECT;
        commit;
        grant default {role_revoke.name} to user {user_01.name};
        commit;

        connect '{act.db.dsn}' user {user_01.name} password '{user_01.password}';
        select current_user as who_am_i,r.rdb$role_name,rdb$role_in_use(r.rdb$role_name),r.rdb$system_privileges
        from mon$database m cross join rdb$roles r;
        commit;

        recreate table test_u01(id int, who_is_author varchar(31) default current_user);
        commit;
        insert into test_u01(id) values(1);
        commit;

        grant select on table test_u01 to {user_02.name}; -- nb: do NOT add here "granted by sysdba"!
        commit;

        -- this should give output with rdb$grantor = 'SYSDBA' despite that actual grantor was '{user_01.name}':
        select * from rdb$user_privileges where rdb$relation_name=upper('test_u01') and rdb$user=upper('{user_02.name}');
        commit;

        connect '{act.db.dsn}' user {user_02.name} password '{user_02.password}';
        select current_user as who_am_i, u.* from test_u01 u;
        commit;

        connect '{act.db.dsn}' user {user_01.name} password '{user_01.password}';
        revoke select on test_u01 from {user_02.name};
        commit;

        set bail off;
        connect '{act.db.dsn}' user {user_02.name} password '{user_02.password}';
        select current_user as who_am_i, u.* from test_u01 u; -- this should FAIL
        commit;
        set bail on;
    """


    expected_stdout_5x = f"""
        WHO_AM_I                        {user_01.name.upper()}
        RDB$ROLE_NAME                   RDB$ADMIN
        RDB$ROLE_IN_USE                 <false>
        RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF
        WHO_AM_I                        {user_01.name.upper()}
        RDB$ROLE_NAME                   {role_revoke.name.upper()}
        RDB$ROLE_IN_USE                 <true>
        RDB$SYSTEM_PRIVILEGES           0000200000000000
        RDB$USER                        {user_02.name.upper()}
        RDB$GRANTOR                     {user_01.name.upper()}
        RDB$PRIVILEGE                   S
        RDB$GRANT_OPTION                0
        RDB$RELATION_NAME               TEST_U01
        RDB$FIELD_NAME                  <null>
        RDB$USER_TYPE                   8
        RDB$OBJECT_TYPE                 0
        WHO_AM_I                        {user_02.name.upper()}
        ID                              1
        WHO_IS_AUTHOR                   {user_01.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE TEST_U01
        -Effective user is {user_02.name.upper()}
    """

    expected_stdout_6x = f"""
        WHO_AM_I                        {user_01.name.upper()}
        RDB$ROLE_NAME                   RDB$ADMIN
        RDB$ROLE_IN_USE                 <false>
        RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF
        WHO_AM_I                        {user_01.name.upper()}
        RDB$ROLE_NAME                   {role_revoke.name.upper()}
        RDB$ROLE_IN_USE                 <true>
        RDB$SYSTEM_PRIVILEGES           0000200000000000
        RDB$USER                        {user_02.name.upper()}
        RDB$GRANTOR                     {user_01.name.upper()}
        RDB$PRIVILEGE                   S
        RDB$GRANT_OPTION                0
        RDB$RELATION_NAME               TEST_U01
        RDB$FIELD_NAME                  <null>
        RDB$USER_TYPE                   8
        RDB$OBJECT_TYPE                 0
        RDB$RELATION_SCHEMA_NAME        PUBLIC
        RDB$USER_SCHEMA_NAME            <null>
        WHO_AM_I                        {user_02.name.upper()}
        ID                              1
        WHO_IS_AUTHOR                   {user_01.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE "PUBLIC"."TEST_U01"
        -Effective user is {user_02.name.upper()}
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
