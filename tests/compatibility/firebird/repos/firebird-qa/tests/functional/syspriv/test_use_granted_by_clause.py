#coding:utf-8

"""
ID:          syspriv.use-granted-by-clause
TITLE:       Check ability to query, modify and deleting data plus add/drop constraints on any table
DESCRIPTION:
    Two users are created, U01 and U02.
    User U01 is granted with system privilege USE_GRANTED_BY_CLAUSE.
    User U02 has NO any privilege.
    User U01 then creates table and issue GRANT SELECT statement for U02 as it was granted by SYSDBA.
    Then we
    1) check result (contrent of RDB$ tables)
    2) connect as U02 and query this table - this should work OK
    3) connect as U01 and revoke grant on just queried table from U02
    4) connect again as U02 and repeat select - this shoiuld fail.
FBTEST:      functional.syspriv.use_granted_by_clause
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user_1 = user_factory('db', name='tmp_syspriv_u01', password='123')
tmp_user_2 = user_factory('db', name='tmp_syspriv_u02', password='456')
tmp_role = role_factory('db', name='role_for_use_granted_by_clause')

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', substitutions = substitutions)

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user_1: User, tmp_user_2: User, tmp_role: Role):

    test_script = f"""
        set wng off;
        set bail on;
        set list on;

        alter user {tmp_user_1.name} revoke admin role;
        alter user {tmp_user_2.name} revoke admin role;
        grant create table to {tmp_user_1.name};
        commit;

        -- Add/change/delete non-system records in RDB$TYPES
        alter role {tmp_role.name} set system privileges to USE_GRANTED_BY_CLAUSE;
        commit;
        grant default {tmp_role.name} to user {tmp_user_1.name};
        commit;

        connect '{act.db.dsn}' user {tmp_user_1.name} password '{tmp_user_1.password}';
        select current_user as who_am_i,r.rdb$role_name,rdb$role_in_use(r.rdb$role_name),r.rdb$system_privileges
        from mon$database m cross join rdb$roles r;
        commit;

        recreate table test_u01(id int, who_is_author varchar(31) default current_user);
        commit;
        insert into test_u01(id) values(1);
        commit;

        grant select on table test_u01 to {tmp_user_2.name} granted by sysdba;
        commit;

        -- this should give output with rdb$grantor = 'SYSDBA' despite that actual grantor was '{tmp_user_1.name}':
        select * from rdb$user_privileges where rdb$relation_name=upper('test_u01') and rdb$user=upper('{tmp_user_2.name}');
        commit;

        connect '{act.db.dsn}' user {tmp_user_2.name} password '{tmp_user_2.password}';
        select current_user as who_am_i, u.* from test_u01 u;
        commit;

        connect '{act.db.dsn}' user {tmp_user_1.name} password '{tmp_user_1.password}';
        revoke select on test_u01 from {tmp_user_2.name} granted by sysdba;
        commit;

        set bail off;
        connect '{act.db.dsn}' user {tmp_user_2.name} password '{tmp_user_2.password}';
        select current_user as who_am_i, u.* from test_u01 u; -- this should FAIL
        commit;
        set bail on;
    """

    expected_stdout_5x = f"""
        WHO_AM_I                        {tmp_user_1.name.upper()}
        RDB$ROLE_NAME                   RDB$ADMIN
        RDB$ROLE_IN_USE                 <false>
        RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF
        WHO_AM_I                        {tmp_user_1.name.upper()}
        RDB$ROLE_NAME                   {tmp_role.name.upper()}
        RDB$ROLE_IN_USE                 <true>
        RDB$SYSTEM_PRIVILEGES           0000100000000000
        RDB$USER                        {tmp_user_2.name.upper()}
        RDB$GRANTOR                     SYSDBA
        RDB$PRIVILEGE                   S
        RDB$GRANT_OPTION                0
        RDB$RELATION_NAME               TEST_U01
        RDB$FIELD_NAME                  <null>
        RDB$USER_TYPE                   8
        RDB$OBJECT_TYPE                 0
        WHO_AM_I                        {tmp_user_2.name.upper()}
        ID                              1
        WHO_IS_AUTHOR                   {tmp_user_1.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE TEST_U01
        -Effective user is {tmp_user_2.name.upper()}
    """

    expected_stdout_6x = f"""
        WHO_AM_I                        {tmp_user_1.name.upper()}
        RDB$ROLE_NAME                   RDB$ADMIN
        RDB$ROLE_IN_USE                 <false>
        RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF
        WHO_AM_I                        {tmp_user_1.name.upper()}
        RDB$ROLE_NAME                   {tmp_role.name.upper()}
        RDB$ROLE_IN_USE                 <true>
        RDB$SYSTEM_PRIVILEGES           0000100000000000
        RDB$USER                        {tmp_user_2.name.upper()}
        RDB$GRANTOR                     SYSDBA
        RDB$PRIVILEGE                   S
        RDB$GRANT_OPTION                0
        RDB$RELATION_NAME               TEST_U01
        RDB$FIELD_NAME                  <null>
        RDB$USER_TYPE                   8
        RDB$OBJECT_TYPE                 0
        RDB$RELATION_SCHEMA_NAME        PUBLIC
        RDB$USER_SCHEMA_NAME            <null>
        WHO_AM_I                        {tmp_user_2.name.upper()}
        ID                              1
        WHO_IS_AUTHOR                   {tmp_user_1.name.upper()}
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE "PUBLIC"."TEST_U01"
        -Effective user is {tmp_user_2.name.upper()}
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
