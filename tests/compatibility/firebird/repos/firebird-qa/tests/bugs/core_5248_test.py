#coding:utf-8

"""
ID:          issue-5527
ISSUE:       5527
TITLE:       Improve consistency in GRANT syntax between roles and privileges according to SQL standard
DESCRIPTION:
NOTES:
    [08.03.2025] pzotov
    1. Removed old test that changed only FB 3.x (no more sense in ti because 3.x is almost at EOL).
    2. Commented out (and will be deleted later) code that expected error when user who was granted role
       with admin option tries to revoke this role from himself. Seince fixed GH-8462 this is NOT so.
    3. Replaced hard-coded names/passwords with variables that are provided by fixtures (tmp_usr*, tmp_role*).
    Checked on 6.0.0.660; 5.0.3.1624; 4.0.6.3189.

    [01.07.2025] pzotov
    Added f-notation into expected_* in order to make proper content in FB 6.x (role name is enclosed in quotes there).
    Checked on 6.0.0.884; 5.0.3.1668; 4.0.6.3214.
"""

import locale

import pytest
from firebird.qa import *

db = db_factory()

tmp_usr0 = user_factory('db', name='tmp$c5248_usr0', password='c5248$u0')
tmp_usr1 = user_factory('db', name='tmp$c5248_usr1', password='c5248$u1')
tmp_usr2 = user_factory('db', name='tmp$c5248_usr2', password='c5248$u2')
tmp_usr3 = user_factory('db', name='tmp$c5248_usr3', password='c5248$u3')
tmp_usr4 = user_factory('db', name='tmp$c5248_usr4', password='c5248$u4')
tmp_role = role_factory('db', name='tmp_role1', do_not_create=True)

substitutions = [('-TMP\\$C5248_USR1 is not grantor of (role|ROLE|Role) on TEST_ROLE1 to TMP\\$C5248_USR1.',
                  '-TMP$C5248_USR1 is not grantor of ROLE on TEST_ROLE1 to TMP$C5248_USR1.'),
                 ('-Effective user is.*', '')]

act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=4.0')
def test_2(act: Action, tmp_usr0: User, tmp_usr1: User, tmp_usr2: User, tmp_usr3: User, tmp_usr4: User, tmp_role: Role):

    test_sql = f"""
        set list on;
        set count on;
        -- #############
        set autoddl OFF;
        -- #############
        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

        recreate view v_grants as
        select
             current_user                    as who_am_i
            ,p.RDB$USER                      as who_was_granted
            ,p.RDB$PRIVILEGE                 as privilege_type
            ,p.RDB$RELATION_NAME             as role_name
            ,r.RDB$OWNER_NAME                as role_owner
            ,p.RDB$GRANTOR                   as granted_by
            ,p.RDB$GRANT_OPTION              as grant_option
        from rdb$user_privileges p
        left join rdb$roles r on p.rdb$relation_name = r.rdb$role_name
        where p.rdb$object_type=13
        ;
        commit;
        grant select on v_grants to public;
        commit;

        grant create role to user {tmp_usr0.name};
        commit;

        connect '{act.db.dsn}' user {tmp_usr0.name} password '{tmp_usr0.password}';
        create role {tmp_role.name}; -- {tmp_usr0.name} is owner of role {tmp_role.name}
        commit;

        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';
        grant {tmp_role.name} to {tmp_usr1.name} with admin option;
        grant {tmp_role.name} to {tmp_usr3.name};
        commit;

        connect '{act.db.dsn}' user {tmp_usr1.name} password '{tmp_usr1.password}';
        grant {tmp_role.name} to {tmp_usr2.name}; ----------------------- {tmp_usr1.name} grants role to {tmp_usr2.name}
        commit;

        -- 1. revoke - avoid cascade grants delete

        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

        select 'Point-1' as msg, v.* from v_grants v where upper(v.who_was_granted) in ( upper('{tmp_usr1.name}'), upper('{tmp_usr2.name}') );  -- must contain 2 records

        revoke {tmp_role.name} from {tmp_usr1.name}; -- Q: whether grant on role '{tmp_role.name}' remains to user '{tmp_usr2.name}' after revoking from '{tmp_usr1.name}' ?

        select 'Point-2' as msg, v.* from v_grants v where upper(v.who_was_granted) in ( upper('{tmp_usr1.name}'), upper('{tmp_usr2.name}') );  -- must contain 1 record for {tmp_usr2.name}

        -- return grant to {tmp_usr1.name} because it was revoked just now:
        rollback;

        /***********************************************
        ############################################
        DISABLED 08.03.2025, after GH-8462 was fixed
        ############################################
        -- 2. revoke: user who has 'admin option' can revoke role from anyone EXCEPT himself
        connect '{act.db.dsn}' user {tmp_usr1.name} password '{tmp_usr1.password}';

        -- Following REVOKE should fail with:
        -- Statement failed, SQLSTATE = 42000
        -- unsuccessful metadata update
        -- -REVOKE failed
        -- -{tmp_usr1.name} is not grantor of Role on {tmp_role.name} to {tmp_usr1.name}.
        revoke {tmp_role.name} from {tmp_usr1.name};

        select * from v_grants where upper(who_was_granted) = upper('{tmp_usr1.name}'); -- record should remain
        rollback;
        ***********************************************/

        -- 3. revoke - check role owner rights
        connect '{act.db.dsn}' user {tmp_usr0.name} password '{tmp_usr0.password}';

        select 'Point-3' as msg, v.* from v_grants v where upper(v.who_was_granted) = upper('{tmp_usr3.name}');

        -- current user = {tmp_usr0.name} - is owner of role {tmp_role.name}, but this role was granted to {tmp_usr3.name} by SYSDBA.
        -- Q: should user '{tmp_usr0.password}' (current) be able to revoke role which he did NOT grant but owns ?
        -- A: yes.

        revoke {tmp_role.name} from {tmp_usr3.name};  -- NO error/warning should be here

        select 'Point-4' as msg, v.* from v_grants v where upper(v.who_was_granted) = upper('{tmp_usr3.name}'); -- record should NOT appear.
        rollback;

        -- 4. revoke - check admin option
        connect '{act.db.dsn}' user {tmp_usr1.name} password '{tmp_usr1.password}';

        select 'Point-5' as msg, v.* from v_grants v where upper(v.who_was_granted) in ( upper('{tmp_usr1.name}'), upper('{tmp_usr3.name}') ); -- two records should be here

        -- current user = {tmp_usr1.name} - is NOT owner of role {tmp_role.name} but he was granted to use it WITH ADMIN option
        -- (grant {tmp_role.name} to {tmp_usr1.name} with admin option).
        -- Q: should user '{tmp_usr1.name}' (current) be able to revoke role which he neither did grant nor owns but has admin option ?
        -- A: yes.

        revoke {tmp_role.name} from {tmp_usr3.name};

        select 'Point-6' as msg, v.* from v_grants v where upper(v.who_was_granted) in (upper('{tmp_usr1.name}'), upper('{tmp_usr3.name}')); -- only one record should be here
        rollback;

        -- 5a. drop role - should fail
        connect '{act.db.dsn}' user {tmp_usr4.name} password '{tmp_usr4.password}';

        -- Statement failed, SQLSTATE = 28000
        -- unsuccessful metadata update
        -- -DROP ROLE {tmp_role.name} failed
        -- -no permission for DROP access to ROLE {tmp_role.name}

        drop role {tmp_role.name}; -- should fail: this user is not owner of this role and he was not granted to use it with admin option

        set count off;
        select count(*) from rdb$roles where rdb$role_name = '{tmp_role.name}';
        set count on;
        rollback;

        connect '{act.db.dsn}' user {tmp_usr0.name} password '{tmp_usr0.password}';

        select 'Point-6' as msg, v.* from v_grants v where upper(v.role_name) = upper('{tmp_role.name}'); -- should output 3 records

        drop role {tmp_role.name}; -- current user: '{tmp_usr0.name}' - is owner of role {tmp_role.name}

        select 'Point-7' as msg, r.* from rdb$database d left join rdb$roles r on upper(r.rdb$role_name) = upper('{tmp_role.name}'); -- should output NULLs

        select 'Point-8' as msg, v.* from rdb$database d left join v_grants v on upper(v.role_name) = upper('{tmp_role.name}'); -- should output NULLs
        rollback;

        -- 6. drop role - check admin option
        connect '{act.db.dsn}' user {tmp_usr1.name} password '{tmp_usr1.password}';

        -- current user: '{tmp_usr1.name}' - HAS grant on role {tmp_role.name} with admin option (but he is NOT owner of this role).

        select 'Point-9' as msg, v.* from v_grants v where upper(v.role_name) = upper('{tmp_role.name}'); -- should output 3 records

        drop role {tmp_role.name}; -- current user: '{tmp_usr0.name}' - is owner of role {tmp_role.name}

        select 'Point-10' as msg, r.* from rdb$database d left join rdb$roles r on upper(r.rdb$role_name) = upper('{tmp_role.name}'); -- should output NULLs
        select 'Point-11' as msg, v.* from rdb$database d left join v_grants v on upper(role_name) = upper('{tmp_role.name}');      -- should output NULLs
        rollback;
    """


    ROLE_NAME = 'TMP_ROLE1' if act.is_version('<6') else '"TMP_ROLE1"'
    act.expected_stdout = f"""
        MSG                             Point-1
        WHO_AM_I                        SYSDBA
        WHO_WAS_GRANTED                 TMP$C5248_USR1
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      SYSDBA
        GRANT_OPTION                    2
        MSG                             Point-1
        WHO_AM_I                        SYSDBA
        WHO_WAS_GRANTED                 TMP$C5248_USR2
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      TMP$C5248_USR1
        GRANT_OPTION                    0
        Records affected: 2
        MSG                             Point-2
        WHO_AM_I                        SYSDBA
        WHO_WAS_GRANTED                 TMP$C5248_USR2
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      TMP$C5248_USR1
        GRANT_OPTION                    0
        Records affected: 1
        MSG                             Point-3
        WHO_AM_I                        TMP$C5248_USR0
        WHO_WAS_GRANTED                 TMP$C5248_USR3
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      SYSDBA
        GRANT_OPTION                    0
        Records affected: 1
        Records affected: 0
        MSG                             Point-5
        WHO_AM_I                        TMP$C5248_USR1
        WHO_WAS_GRANTED                 TMP$C5248_USR1
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      SYSDBA
        GRANT_OPTION                    2
        MSG                             Point-5
        WHO_AM_I                        TMP$C5248_USR1
        WHO_WAS_GRANTED                 TMP$C5248_USR3
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      SYSDBA
        GRANT_OPTION                    0
        Records affected: 2
        MSG                             Point-6
        WHO_AM_I                        TMP$C5248_USR1
        WHO_WAS_GRANTED                 TMP$C5248_USR1
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      SYSDBA
        GRANT_OPTION                    2
        Records affected: 1
        Statement failed, SQLSTATE = 28000
        unsuccessful metadata update
        -DROP ROLE TMP_ROLE1 failed
        -no permission for DROP access to ROLE {ROLE_NAME}
        COUNT                           1
        MSG                             Point-6
        WHO_AM_I                        TMP$C5248_USR0
        WHO_WAS_GRANTED                 TMP$C5248_USR1
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      SYSDBA
        GRANT_OPTION                    2
        MSG                             Point-6
        WHO_AM_I                        TMP$C5248_USR0
        WHO_WAS_GRANTED                 TMP$C5248_USR3
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      SYSDBA
        GRANT_OPTION                    0
        MSG                             Point-6
        WHO_AM_I                        TMP$C5248_USR0
        WHO_WAS_GRANTED                 TMP$C5248_USR2
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      TMP$C5248_USR1
        GRANT_OPTION                    0
        Records affected: 3
        MSG                             Point-7
        RDB$ROLE_NAME                   <null>
        RDB$OWNER_NAME                  <null>
        RDB$DESCRIPTION                 <null>
        RDB$SYSTEM_FLAG                 <null>
        RDB$SECURITY_CLASS              <null>
        RDB$SYSTEM_PRIVILEGES           <null>
        Records affected: 1
        MSG                             Point-8
        WHO_AM_I                        <null>
        WHO_WAS_GRANTED                 <null>
        PRIVILEGE_TYPE                  <null>
        ROLE_NAME                       <null>
        ROLE_OWNER                      <null>
        GRANTED_BY                      <null>
        GRANT_OPTION                    <null>
        Records affected: 1
        MSG                             Point-9
        WHO_AM_I                        TMP$C5248_USR1
        WHO_WAS_GRANTED                 TMP$C5248_USR1
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      SYSDBA
        GRANT_OPTION                    2
        MSG                             Point-9
        WHO_AM_I                        TMP$C5248_USR1
        WHO_WAS_GRANTED                 TMP$C5248_USR3
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      SYSDBA
        GRANT_OPTION                    0
        MSG                             Point-9
        WHO_AM_I                        TMP$C5248_USR1
        WHO_WAS_GRANTED                 TMP$C5248_USR2
        PRIVILEGE_TYPE                  M
        ROLE_NAME                       TMP_ROLE1
        ROLE_OWNER                      TMP$C5248_USR0
        GRANTED_BY                      TMP$C5248_USR1
        GRANT_OPTION                    0
        Records affected: 3
        MSG                             Point-10
        RDB$ROLE_NAME                   <null>
        RDB$OWNER_NAME                  <null>
        RDB$DESCRIPTION                 <null>
        RDB$SYSTEM_FLAG                 <null>
        RDB$SECURITY_CLASS              <null>
        RDB$SYSTEM_PRIVILEGES           <null>
        Records affected: 1
        MSG                             Point-11
        WHO_AM_I                        <null>
        WHO_WAS_GRANTED                 <null>
        PRIVILEGE_TYPE                  <null>
        ROLE_NAME                       <null>
        ROLE_OWNER                      <null>
        GRANTED_BY                      <null>
        GRANT_OPTION                    <null>
        Records affected: 1
    """
    act.isql(switches = ['-q'], input = test_sql, combine_output = True, connect_db = False, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
