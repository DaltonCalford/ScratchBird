#coding:utf-8

"""
ID:          issue-2245
ISSUE:       2245
TITLE:       Ability to grant role to another role
DESCRIPTION:
    ### NB ### This test was NOT completed!
    One need to check all nuances related to granting and revoking that is issued by NON sysdba user
    which was granted admin option to manupulate appropriate DB objects.
    Also, there is not clarity about issue 08/Aug/16 06:31 AM (when user Boss2 does grant-and-revoke
    sequence with some role to other user Sales but this role already was granted by _other_ user, Boss1).

    We create two users (acnt and pdsk) and two roles for them (racnt and rpdsk).
    Then we create two tables (tacnt & tpdsk) and grant access on these tables for acnt & pdsk.
    Then we create user boss, role for him (rboss) and grant IMPLICITLY access on tables tacnt and tpdsk
    to user boss via his role (rboss).
    Check is made to ensure that user boss HAS ability to read from both tables (being connected with role Rboss).
    After all, we IMPLICITLY revoke access from these tables (by revoking ROLES from boss) and check again that
    user boss now has NO access on tables tacnt and tpdsk.
JIRA:        CORE-1815
FBTEST:      bugs.core_1815
NOTES:
    [26.06.2025] pzotov
    Re-implemented: use fixture attributes and f-notation instead of hard-coding user/role names.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

user_boss = user_factory('db', name='tmp$c1815_boss', password='boss')
user_acnt = user_factory('db', name='tmp$c1815_acnt', password='acnt')
user_pdsk = user_factory('db', name='tmp$c1815_pdsk', password='pdsk')

role_boss = role_factory('db', name='role_boss')
role_acnt = role_factory('db', name='role_acnt')
role_pdsk = role_factory('db', name='role_pdsk')


substitutions = [] # [('[ \t]+', ' ')]
act = isql_act('db', substitutions = substitutions)

@pytest.mark.version('>=4.0')
def test_1(act: Action, user_boss: User, user_acnt: User, user_pdsk: User, role_boss: Role, role_acnt: Role, role_pdsk: Role):

    test_script = f"""
        set wng off;
        set list on;

        set width whoami 16;
        set width my_role 7;
        set width r_name 10;
        set width r_owner 10;

        create or alter view v_role_info as
        select
            current_user as whoami,
            current_role as my_role,
            rdb$role_name as r_name,
            rdb$owner_name r_owner,
            rdb$role_in_use(rdb$role_name) r_in_use
        from rdb$roles
        where coalesce(rdb$system_flag,0) = 0
        ;
        commit;

        grant select on v_role_info to user {user_acnt.name};
        grant select on v_role_info to user {user_pdsk.name};
        grant select on v_role_info to user {user_boss.name};
        commit;

        grant {role_acnt.name} to user {user_acnt.name};
        grant {role_pdsk.name} to user {user_pdsk.name};
        commit;

        ------------------------------------------------------------------------

        recreate table tacnt(id int, s varchar(15));
        recreate table tpdsk(id int, s varchar(15));
        commit;

        grant all on tacnt to role {role_acnt.name};
        grant all on tpdsk to role {role_pdsk.name};
        commit;

        ------------------------------------------------------------------------

        grant {role_acnt.name} to {role_boss.name};           -- make {role_boss.name} role able to do the same as role {role_acnt.name}
        grant {role_pdsk.name} to {role_boss.name};           -- make {role_boss.name} role able to do the same as role {role_pdsk.name}

        grant {role_boss.name} to {user_boss.name};  -- let user BOSS to use role {role_boss.name}

        commit;
        --show grants; -- [ 1 ]

        ------------------------------------------------------------------------

        insert into tacnt(id, s) values(1, '{role_acnt.name.upper()}');
        insert into tpdsk(id, s) values(2, '{role_pdsk.name.upper()}');
        commit;

        connect '{act.db.dsn}' user {user_acnt.name} password '{user_acnt.password}' role '{role_acnt.name}';
        select * from v_role_info;
        select current_user as whoami, a.* from tacnt a;
        commit;

        connect '{act.db.dsn}' user {user_pdsk.name} password '{user_pdsk.password}' role '{role_pdsk.name}';
        select * from v_role_info;
        select current_user as whoami, p.* from tpdsk p;
        commit;


        connect '{act.db.dsn}' user {user_boss.name} password '{user_boss.password}' role '{role_boss.name}';
        select * from v_role_info;
        commit;

        select current_user as whoami, a.* from tacnt a;
        select current_user as whoami, p.* from tpdsk p;
        commit;

        --################################################################################################

        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

        revoke {role_acnt.name} from user {user_acnt.name}; -- REVOKE role from *other* USER; grant to {role_boss.name} should be preserved!
        commit;

        -- check that *role* {role_boss.name} still HAS grants to both {role_acnt.name} and {role_pdsk.name} roles:
        connect '{act.db.dsn}' user {user_boss.name} password '{user_boss.password}' role '{role_boss.name}';

        select * from v_role_info; -- should contain <true> in all three rows

        -- should PASS because we revoked role from other USER (accountant)
        -- rather than from current user (Boss) or its role ({role_boss.name}):
        select current_user as whoami, a.* from tacnt a;
        commit;

        --####################################################################################################

        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

        -- check that if we try to revoke role {role_pdsk.name} from __USER__ Boss than
        -- this action will not has effect because this USER got access through the ROLE (i.e. indirectly):

        revoke {role_pdsk.name} from user {user_boss.name}; -- this is no-op action: we did NOT granted role to USER.
        commit;

        connect '{act.db.dsn}' user {user_boss.name} password '{user_boss.password}' role '{role_boss.name}'; -- now check: is role {role_boss.name} really affected ?

        select * from v_role_info; -- should contain <true> in all lines because we did not affect __role__ {role_boss.name}
        select current_user as whoami, p.* from tpdsk p; -- should PASS!
        commit;

        --################################################################################################

        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

        -- check that if we revoke access to a table from ROLE {role_pdsk.name} (and this role was granted to role {role_boss.name})
        -- then {role_boss.name} also will not be able to select from this table:

        revoke all on tpdsk from {role_pdsk.name};
        commit;

        connect '{act.db.dsn}' user {user_boss.name} password '{user_boss.password}' role '{role_boss.name}';

        select * from v_role_info; -- should contain <true> in all lines because we did not affect __role__ {role_boss.name}
        select current_user as whoami, p.* from tpdsk p; -- should FAIL
        commit;

        --################################################################################################

        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

        -- check that if we revoke ROLE '{role_acnt.name}' which was granted before to ROLE '{role_boss.name}'
        -- then user Boss will not be able to access table 'tacnt' (i.e. we revoke this access indirectly):

        revoke {role_acnt.name} from {role_boss.name};
        commit;

        connect '{act.db.dsn}' user {user_boss.name} password '{user_boss.password}' role '{role_boss.name}';

        select * from v_role_info; -- should contain <false> for line with '{role_acnt.name}'
        select current_user as whoami, a.* from tacnt a; -- should FAIL
        commit;

        -- ###############################################################################################

        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

        -- check that if we GRANT again ROLE '{role_acnt.name}' which was revoked before from ROLE '{role_boss.name}'
        -- then user Boss WILL be able to access table 'tacnt' (we grant access indirectly after revoking):

        grant {role_acnt.name} to {role_boss.name}; -- RESTORE access for role {role_boss.name}
        commit;


        connect '{act.db.dsn}' user {user_boss.name} password '{user_boss.password}' role '{role_boss.name}';

        select * from v_role_info; -- should contain <true> for line with '{role_acnt.name}'
        select current_user as whoami, a.* from tacnt a; -- should PASS
        commit;
    """


    expected_stdout_5x = f"""
        WHOAMI                          {user_acnt.name.upper()}
        MY_ROLE                         {role_acnt.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <false>
        WHOAMI                          {user_acnt.name.upper()}
        MY_ROLE                         {role_acnt.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_acnt.name.upper()}
        MY_ROLE                         {role_acnt.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <false>
        WHOAMI                          {user_acnt.name.upper()}
        ID                              1
        S                               {role_acnt.name.upper()}
        WHOAMI                          {user_pdsk.name.upper()}
        MY_ROLE                         {role_pdsk.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <false>
        WHOAMI                          {user_pdsk.name.upper()}
        MY_ROLE                         {role_pdsk.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <false>
        WHOAMI                          {user_pdsk.name.upper()}
        MY_ROLE                         {role_pdsk.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_pdsk.name.upper()}
        ID                              2
        S                               {role_pdsk.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        ID                              1
        S                               {role_acnt.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        ID                              2
        S                               {role_pdsk.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        ID                              1
        S                               {role_acnt.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        ID                              2
        S                               {role_pdsk.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE TPDSK
        -Effective user is {user_boss.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <false>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE TACNT
        -Effective user is {user_boss.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        ID                              1
        S                               {role_acnt.name.upper()}
    """

    expected_stdout_6x = f"""
        WHOAMI                          {user_acnt.name.upper()}
        MY_ROLE                         {role_acnt.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <false>
        WHOAMI                          {user_acnt.name.upper()}
        MY_ROLE                         {role_acnt.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_acnt.name.upper()}
        MY_ROLE                         {role_acnt.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <false>
        WHOAMI                          {user_acnt.name.upper()}
        ID                              1
        S                               {role_acnt.name.upper()}
        WHOAMI                          {user_pdsk.name.upper()}
        MY_ROLE                         {role_pdsk.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <false>
        WHOAMI                          {user_pdsk.name.upper()}
        MY_ROLE                         {role_pdsk.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <false>
        WHOAMI                          {user_pdsk.name.upper()}
        MY_ROLE                         {role_pdsk.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_pdsk.name.upper()}
        ID                              2
        S                               {role_pdsk.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        ID                              1
        S                               {role_acnt.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        ID                              2
        S                               {role_pdsk.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        ID                              1
        S                               {role_acnt.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        ID                              2
        S                               {role_pdsk.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE "PUBLIC"."TPDSK"
        -Effective user is {user_boss.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <false>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        Statement failed, SQLSTATE = 28000
        no permission for SELECT access to TABLE "PUBLIC"."TACNT"
        -Effective user is {user_boss.name.upper()}
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_boss.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_acnt.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        MY_ROLE                         {role_boss.name.upper()}
        R_NAME                          {role_pdsk.name.upper()}
        R_OWNER                         {act.db.user.upper()}
        R_IN_USE                        <true>
        WHOAMI                          {user_boss.name.upper()}
        ID                              1
        S                               {role_acnt.name.upper()}
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
