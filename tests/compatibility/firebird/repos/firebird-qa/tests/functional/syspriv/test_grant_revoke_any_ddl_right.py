#coding:utf-8

"""
ID:          syspriv.grant-revoke-any-ddl-right
TITLE:       Check ability to grant right for issuing CREATE/ALTER/DROP statements
DESCRIPTION:
    Test creates user <tmp_user_grantor> and grants to him system privilege
    to allow another user (<tmp_user_grantee>) to run any DDL statement, and also
    to revoke all privileges from  this user. DDLs will be run by <tmp_user_grantee>.
    
    After this, we connect as <tmp_user_grantor> and give all kinds of DDL rights
    for CREATE, ALTER and DROP objects to user <tmp_user_grantee>.

    We then connect to database as <tmp_user_grantee> and try to create all kind of
    database objects, then alter and drop them. No errors must occur here.

    Finally, we make connect as <tmp_user_grantor> and revoke from <tmp_user_grantee>
    all grants. User <tmp_user_grantee> then makes connect and tries to CREATE any kind
    of DB objects. All of them must NOT be created and exception SQLSTATE = 42000 must raise.
NOTES:
    [12.07.2025] pzotov
    Re-implemented: removed hard-coded names; changed code to be able to run with SQL schemas
    that appeared in 6.0.0.834 (note that 'grant ater any character set' in 6.x requires a new
    clause 'ON SCHEMA' - see `ON_SYSTEM_SCHEMA_CLAUSE` and comments in the text below).
    Checked on 6.0.0.949; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user_grantor = user_factory('db', name='senior_ddl_grantor', password = '123')
tmp_user_grantee = user_factory('db', name='junior_ddl_grantee', password = '456')
tmp_role = role_factory('db', name='r_for_grant_revoke_any_ddl_right')

act = isql_act('db', substitutions = [('"', '')])

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_user_grantor: User, tmp_user_grantee: User, tmp_role: Role):

    # need for 'grant alter any character set' see doc/sql.extensions/README.schemas.md 
    # 'grant alter any procedure on schema SCHEMA1 to PUBLIC;' etc
    ON_SYSTEM_SCHEMA_CLAUSE = '' if act.is_version('<6') else 'ON SCHEMA SYSTEM'
    test_script = f"""
        set wng off;
        set bail on;
        set list on;

        alter user {tmp_user_grantor.name} revoke admin role;
        alter user {tmp_user_grantee.name} revoke admin role;
        commit;

        -- Add/change/delete non-system records in RDB$TYPES
        alter role {tmp_role.name} set system privileges to GRANT_REVOKE_ANY_DDL_RIGHT;
        commit;
        grant default {tmp_role.name} to user {tmp_user_grantor.name};
        commit;

        connect '{act.db.dsn}' user {tmp_user_grantor.name} password '123';
        select current_user as who_am_i,r.rdb$role_name,rdb$role_in_use(r.rdb$role_name),r.rdb$system_privileges
        from mon$database m cross join rdb$roles r;
        commit;

        -- set echo on;
        -- ### NOTE ###
        -- We give this system privilege being connected as '{tmp_user_grantor.name}', NOT as SYSDBA!
        grant alter any character set {ON_SYSTEM_SCHEMA_CLAUSE} to {tmp_user_grantee.name};

        grant create collation to {tmp_user_grantee.name};
        grant alter any collation to {tmp_user_grantee.name};
        grant drop any collation to {tmp_user_grantee.name};

        grant create exception to {tmp_user_grantee.name};
        grant alter any exception to {tmp_user_grantee.name};
        grant drop any exception to {tmp_user_grantee.name};

        grant create generator to {tmp_user_grantee.name};
        grant alter any generator to {tmp_user_grantee.name};
        grant drop any generator to {tmp_user_grantee.name};

        grant create domain to {tmp_user_grantee.name};
        grant alter any domain to {tmp_user_grantee.name};
        grant drop any domain to {tmp_user_grantee.name};

        grant create role to {tmp_user_grantee.name};
        grant alter any role to {tmp_user_grantee.name};
        grant drop any role to {tmp_user_grantee.name};

        -- DDL operations for managing triggers and indices re-use table privileges.
        -- Ability to add COMMENT on some object requires ALTER ANY privilege for this kind of objects.
        grant create table to {tmp_user_grantee.name};
        grant alter any table to {tmp_user_grantee.name};
        grant drop any table to {tmp_user_grantee.name};

        grant create view to {tmp_user_grantee.name};
        grant alter any view to {tmp_user_grantee.name};
        grant drop any view to {tmp_user_grantee.name};

        grant create procedure to {tmp_user_grantee.name};
        grant alter any procedure to {tmp_user_grantee.name};
        grant drop any procedure to {tmp_user_grantee.name};

        grant create function to {tmp_user_grantee.name};
        grant alter any function to {tmp_user_grantee.name};
        grant drop any function to {tmp_user_grantee.name};

        grant create package to {tmp_user_grantee.name};
        grant alter any package to {tmp_user_grantee.name};
        grant drop any package to {tmp_user_grantee.name};

        commit;

        -- this should give output with rdb$grantor = 'SYSDBA' despite that actual grantor was '{tmp_user_grantor.name}':
        -- select * from rdb$user_privileges where rdb$user=upper('{tmp_user_grantee.name}');
        -- commit;

        connect '{act.db.dsn}' user {tmp_user_grantee.name} password '{tmp_user_grantee.password}';
        select current_user, current_role from rdb$database;
        --############################################################################
        --###   v e r i f y    r i g h t    t o     C R E A T E    o b j e c t s   ###
        --############################################################################
        select 'Verify that user has permissions to CREATE objects of different types.' msg, current_user from rdb$database;

        create collation coll_test for utf8 from unicode case insensitive;
        create exception exc_test 'Invalud value: @1';
        create sequence gen_test;
        create domain dm_test as int;
        create role r_test;
        create table table_test(id int, pid int, x int, constraint mtest_pk primary key(id), constraint m_test_fk foreign key(pid) references table_test(id));
        create index table_test_x_asc on table_test(x);
        create trigger table_test_trg for table_test before insert sql security invoker as begin end;
        create view v_table_test as select * from table_test;
        set term ^;
        create procedure sp_test(a_id int) returns(x int) as
        begin
            suspend;
        end
        ^
        create function fn_test returns int as
        begin
            return 1;
        end
        ^
        create package pg_test as
        begin
           procedure pg_sp1(a_id int);
           function pg_fn1 returns int;
        end
        ^
        create package body pg_test as
        begin
           procedure pg_sp1(a_id int) as
           begin
           end

           function pg_fn1 returns int as
           begin
               return 1;
           end
        end
        ^
        set term ;^
        select 'Passed.' msg, current_user from rdb$database;
        commit;

        --###################################################################################
        --###   v e r i f y    r i g h t    t o     A L T E R   A N Y     o b j e c t s   ###
        --###################################################################################
        select 'Verify that user has permissions to ALTER ANY object.' msg, current_user from rdb$database;
        
        -- NB: on 6.x one need to use `ON SCHEMA` clause in GRANT ALTER ANY CHARSET statement,
        -- i.e. `grant alter any character set on schema system to junior;`
        -- See also reply from Adriano, 03-JUL-2025 14:59
        -- subj: "Regression (?) in 6.x: 'ALTER CHAR SET SET DEFAULT COLLATION ...' not allowed"
        -- See also see doc/sql.extensions/README.schemas.md:
        -- 'grant alter any procedure on schema SCHEMA1 to PUBLIC;' etc

        alter character set iso8859_1 set default collation pt_br;

        alter exception exc_test 'You have to change value from @1 to @2';
        alter sequence gen_test restart with -9223372036854775808 increment by 2147483647;
        alter domain dm_test type bigint set default 2147483647 set not null add check(value > 0);

        alter table table_test drop constraint m_test_fk;
        create descending index table_test_x_desc on table_test(x);
        comment on table table_test is 'New comment for this table.';
        set term ^;
        alter trigger table_test_trg inactive after insert or update or delete sql security definer as
            declare c bigint;
        begin
            c = gen_id(gen_test,1);
        end
        ^
        alter view v_table_test as select x.id from rdb$database r left join table_test x on 1=1
        ^

        alter procedure sp_test(a_id int) returns(x int, z bigint) as
        begin
            suspend;
        end
        ^
        alter function fn_test returns bigint as
        begin
            return -9223372036854775808;
        end
        ^
        alter package pg_test as
        begin
           procedure pg_sp1(a_id bigint) returns(z bigint);
           function pg_fn1(a_id bigint) returns bigint;
        end
        ^
        recreate package body pg_test as
        begin
           procedure pg_sp1(a_id bigint) returns(z bigint) as
           begin
               z = a_id * 2;
               suspend;
           end

           function pg_fn1(a_id bigint) returns bigint as
           begin
               return a_id * 3;
           end
        end
        ^
        set term ;^
        select 'Passed.' msg, current_user from rdb$database;
        commit;

        --################################################################################
        --###   v e r i f y    r i g h t    t o    D R O P    A N Y    o b j e c t s   ###
        --################################################################################
        select 'Verify that user has permissions to DROP ANY object.' msg, current_user from rdb$database;
        drop package body pg_test;
        drop package pg_test;
        drop procedure sp_test;
        drop function fn_test;
        drop view v_table_test;
        drop index table_test_x_asc;
        drop trigger table_test_trg;
        drop table table_test;
        drop domain dm_test;
        drop sequence gen_test;
        drop exception exc_test;
        drop collation coll_test;
        select 'Passed.' msg, current_user from rdb$database;
        commit;

        --######################################################
        --###   r e v o k e    a l l    p r i v i l e g e s  ###
        --######################################################
        connect '{act.db.dsn}' user {tmp_user_grantor.name} password '{tmp_user_grantor.password}';
        revoke all on all from {tmp_user_grantee.name};
        commit;

        set bail off;

        connect '{act.db.dsn}' user {tmp_user_grantee.name} password '{tmp_user_grantee.password}';

        select 'Verify that no permissions remain' msg, current_user from rdb$database;
        --###########################################################################
        --###   v e r i f y    t h a t     N O    r i g h t s    r e m a i n s    ###
        --###########################################################################
        -- ALL FOLLOWING STATEMENTS MUST FAIL NOW BECAUSE CURRENT USER
        -- HAS NO RIGHTS TO CREATE/ALTER/DROP ANY OBJECTS:
        create collation coll_test2 for utf8 from unicode case insensitive; -- must FAIL!
        create exception exc_test2 'Invalud value: @1';
        create sequence gen_test2;
        create domain dm_test2 as int;
        create role r_test2;
        create table table_test2(id int, pid int, x int, constraint mtest_pk primary key(id), constraint m_test_fk foreign key(pid) references table_test(id));
        create view v_table_test2 as select 1 from rdb$database;

        set term ^;
        create procedure sp_test2 as begin end
        ^
        create function fn_test2 returns boolean as begin return false; end
        ^
        create package pg_test2 as begin
            procedure pg_sp2;
        end
        ^
        create package body pg_test2 as begin
            procedure pg_sp2 as begin end
        end
        ^
        set term ;^
        commit;
    """

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    expected_stdout = f"""
        WHO_AM_I                        {tmp_user_grantor.name.upper()}
        RDB$ROLE_NAME                   RDB$ADMIN
        RDB$ROLE_IN_USE                 <false>
        RDB$SYSTEM_PRIVILEGES           FFFFFFFFFFFFFFFF
        WHO_AM_I                        {tmp_user_grantor.name.upper()}
        RDB$ROLE_NAME                   {tmp_role.name.upper()}
        RDB$ROLE_IN_USE                 <true>
        RDB$SYSTEM_PRIVILEGES           0000400000000000
        USER                            {tmp_user_grantee.name.upper()}
        ROLE                            NONE
        MSG                             Verify that user has permissions to CREATE objects of different types.
        USER                            {tmp_user_grantee.name.upper()}
        MSG                             Passed.
        USER                            {tmp_user_grantee.name.upper()}
        MSG                             Verify that user has permissions to ALTER ANY object.
        USER                            {tmp_user_grantee.name.upper()}
        MSG                             Passed.
        USER                            {tmp_user_grantee.name.upper()}
        MSG                             Verify that user has permissions to DROP ANY object.
        USER                            {tmp_user_grantee.name.upper()}
        MSG                             Passed.
        USER                            {tmp_user_grantee.name.upper()}
        MSG                             Verify that no permissions remain
        USER                            {tmp_user_grantee.name.upper()}
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE COLLATION {SQL_SCHEMA_PREFIX}"COLL_TEST2" failed
        -No permission for CREATE COLLATION operation
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE EXCEPTION {SQL_SCHEMA_PREFIX}"EXC_TEST2" failed
        -No permission for CREATE EXCEPTION operation
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE SEQUENCE {SQL_SCHEMA_PREFIX}"GEN_TEST2" failed
        -No permission for CREATE GENERATOR operation
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE DOMAIN {SQL_SCHEMA_PREFIX}"DM_TEST2" failed
        -No permission for CREATE DOMAIN operation
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE ROLE R_TEST2 failed
        -No permission for CREATE ROLE operation
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE TABLE {SQL_SCHEMA_PREFIX}"TABLE_TEST2" failed
        -No permission for CREATE TABLE operation
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE VIEW {SQL_SCHEMA_PREFIX}"V_TABLE_TEST2" failed
        -No permission for CREATE VIEW operation
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE PROCEDURE {SQL_SCHEMA_PREFIX}"SP_TEST2" failed
        -No permission for CREATE PROCEDURE operation
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE FUNCTION {SQL_SCHEMA_PREFIX}"FN_TEST2" failed
        -No permission for CREATE FUNCTION operation
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE PACKAGE {SQL_SCHEMA_PREFIX}"PG_TEST2" failed
        -No permission for CREATE PACKAGE operation
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE PACKAGE BODY {SQL_SCHEMA_PREFIX}"PG_TEST2" failed
        -No permission for CREATE PACKAGE operation
    """

    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
