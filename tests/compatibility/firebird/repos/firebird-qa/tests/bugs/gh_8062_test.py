#coding:utf-8

"""
ID:          issue-8062
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8062
TITLE:       CREATE [IF NOT EXISTS]
DESCRIPTION:
    Test uses pre-created databases.conf which has alias (see variable REQUIRED_ALIAS) and SecurityDatabase in its details
    which points to that alias, thus making such database be self-security.
    Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
    
    NOTE.
    This database MUST be self-secutity because test creates *GLOBAL* mapping which must not be written to default security.db
    
    We create objects of all types which are enumerated in doc to be avaliable for 'CREATE [IF NOT EXISTS]' statement, and also we
    create DDL triggers for log appropriate activity in the table 'log_ddl_triggers_activity'.
    Then we run CREATE IF NOT EXISTS statements:
        * for NON-existing objects (this MUST be logged).
        * for existing objects (this must NOT be logged)
    Also, we check 'ALTER TABLE ADD COLUMN IF NOT EXISTS' for combination of existing and non-existing columns (it must be logged).
    Finally, content of table 'log_ddl_triggers_activity' is checked.
    Every issued DDL statement must be logged FOUR times: two by before- and after-triggers for this event and two by 'universal'
    triggers for ANY DDL STATEMENT.

NOTES:
    [15.04.2024] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    4. It is crucial to be sure that current OS environment has no ISC_USER and ISC_PASSWORD variables. Test forcibly unsets them.
    5. 'CREATE USER IF NOT EXISTS' currently not checked because DDL trigger *will* be fire for that command if user does exist.

    Checked on Windows, 6.0.0.315 SS/CS, intermediate snapshot on commit #003b2e0.
"""

import os
import re
import locale
from pathlib import Path
import time

import pytest
from firebird.qa import *

substitutions = [('[ \t]+', ' '), ]

REQUIRED_ALIAS = 'tmp_gh_8062_alias'

# MANDATORY! OTHERWISE ISC_ variables will take precedense over credentials = False!
for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

db = db_factory()
act = python_act('db', substitutions=substitutions)


@pytest.mark.version('>=6.0')
def test_1(act: Action, capsys):

    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_8062_alias = $(dir_sampleDb)/qa/tmp_qa_8062.fdb 
                # - then we extract filename: 'tmp_qa_8062.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    tmp_dba_pswd = 'p@$$w8062'
    check_sql = f"""
        -- DO NOT: set bail on; -- we have to drop database at final point!
        set list on;
        -- /*
        rollback;
        create database '{REQUIRED_ALIAS}';
        alter database set linger to 0;
        create user {act.db.user} password '{tmp_dba_pswd}' using plugin Srp;
        commit;
        connect '{REQUIRED_ALIAS}' user {act.db.user};
        -- */
        select mon$sec_database from mon$database; -- must be: 'Self'
        commit;

        drop global mapping if exists map_global_existent;
        drop user if exists u_gh_8062_existent;

        -- ####################################################
        -- INITIAL creation of DB objects (before DDL triggers)
        -- ####################################################
        create mapping map_local_existent using plugin Srp from any user to user;
        create global mapping map_global_existent using plugin Srp from any user to user;
        create user u_gh_8062_existent password '123';
        create role role_existent;
        create domain dm_existent as int;
        create sequence gen_existent;
        create exception exc_existent 'foo';
        create collation coll_existent for utf8 from unicode;

        create table log_ddl_triggers_activity (
            id int generated by default as identity constraint pk_log_ddl_triggers_activity primary key
            ,ddl_trigger_name varchar(64)
            ,event_type varchar(25) not null
            ,object_type varchar(25) not null
            ,ddl_event varchar(25) not null
            ,object_name varchar(64) not null
            ,dts timestamp default 'now'
            ,running_ddl varchar(8190)
        );

        create view v_detailed_ddl_log as
        select 
            id
            ,running_ddl
            ,ddl_trigger_name
            ,event_type
            ,object_type
            ,ddl_event
            ,object_name
            ,count(*)over(partition by ddl_event, object_name) as fired_ddl_trg_count
        from log_ddl_triggers_activity
        order by id;

        create view v_check_ddl_log as
        select 
            id
            ,ddl_trigger_name
            ,event_type
            ,object_type
            ,ddl_event
            ,object_name
            ,fired_ddl_trg_count
        from v_detailed_ddl_log
        ;

        create table es_list(
            id int generated by default as identity constraint pk_es_list primary key
           ,sttm varchar(8190)
        );

        create table t_existent (
            id int primary key
            ,pid int
            ,f01_existent int
            ,f02_existent int
            ,f03_existent int
            ,constraint t_existent_fk foreign key(pid) references t_existent(id) on delete cascade
        );
        create index t_existent_f01 on t_existent(f01_existent);
        create view v_existent as select * from t_existent;

        create table t_one_else_existent (
            id int primary key
            ,pid int
            ,f01_one_else_existent int
            ,f02_one_else_existent int
            ,f03_one_else_existent int
        );


        set term ^;
        create trigger trg_existent for t_existent before insert as
        begin
        end
        ^
        create procedure sp_existent as
        begin
        end
        ^
        create function fn_existent returns int as
        begin
          return 1;
        end
        ^
        create package pg_existent as
        begin
            procedure p;
            function f returns int;
        end
        ^
        create package body pg_existent as
        begin
            procedure p as
            begin
            end
            function f returns int as
            begin
                return 1;
            end
        end
        ^

        -- This package initially has NO body.
        -- We will use it for 'create package body if not exists' -- see below:
        create package pg_missed_implementation as
        begin
            procedure p_missed;
            function f_missed returns int;
        end
        ^
        commit
        ^

        -- ###################
        -- create DDL triggers
        -- ###################
        execute block as
            declare v_lf char(1) = x'0A';
        begin
            rdb$set_context('USER_SESSION', 'SKIP_DDL_TRIGGER', '1');

            for
                with
                a as (
                    select 'ANY DDL STATEMENT' x from rdb$database union all
                    select 'ALTER TABLE' from rdb$database union all
                    select 'CREATE MAPPING' from rdb$database union all
                    select 'CREATE TABLE' from rdb$database union all
                    select 'CREATE PROCEDURE' from rdb$database union all
                    select 'CREATE FUNCTION' from rdb$database union all
                    select 'CREATE TRIGGER' from rdb$database union all
                    select 'CREATE EXCEPTION' from rdb$database union all
                    select 'CREATE VIEW' from rdb$database union all
                    select 'CREATE DOMAIN' from rdb$database union all
                    select 'CREATE ROLE' from rdb$database union all
                    select 'CREATE SEQUENCE' from rdb$database union all
                    select 'CREATE USER' from rdb$database union all
                    select 'CREATE INDEX' from rdb$database union all
                    select 'CREATE COLLATION' from rdb$database union all
                    select 'CREATE PACKAGE' from rdb$database union all
                    select 'CREATE PACKAGE BODY' from rdb$database
                )
                ,e as (
                    select 'before' w from rdb$database union all select 'after' from rdb$database
                )
                ,t as (
                    select upper(trim(replace(trim(a.x),' ','_')) || iif(e.w='before', '_before', '_after')) as trg_name, a.x, e.w
                    from e, a
                )

                select
                       'create trigger trg_' || t.trg_name
                    || ' active ' || t.w || ' ' || trim(t.x) || ' as '
                    || :v_lf
                    || 'begin'
                    || :v_lf
                    || q'!    if (rdb$get_context('USER_SESSION', 'SKIP_DDL_TRIGGER') is null) then!'
                    || :v_lf
                    || '        insert into log_ddl_triggers_activity(ddl_trigger_name, event_type, object_type, ddl_event, object_name, running_ddl) values('
                    || :v_lf
                    || q'!'!' || trim(t.trg_name) || q'!'!'
                    || :v_lf
                    || q'!, rdb$get_context('DDL_TRIGGER', 'EVENT_TYPE')!'
                    || :v_lf
                    || q'!, rdb$get_context('DDL_TRIGGER', 'OBJECT_TYPE')!'
                    || :v_lf
                    || q'!, rdb$get_context('DDL_TRIGGER', 'DDL_EVENT')!'
                    || :v_lf
                    || q'!, rdb$get_context('DDL_TRIGGER', 'OBJECT_NAME')!'
                    || :v_lf
                    || q'!, rdb$get_context('USER_SESSION', 'RUNNING_DDL')!'
                    || :v_lf
                    || ');'
                    || :v_lf
                    || ' end'
                    as sttm
                from t
                as cursor c
            do begin
                 execute statement(c.sttm) with autonomous transaction;
            end

            rdb$set_context('USER_SESSION', 'SKIP_DDL_TRIGGER', null);
        end
        ^
        commit
        ^
        set term ;^


        -- ##########################
        -- RUN 'CREATE IF NOT EXISTS' (DDL triggers must log actions if object did not exists before that)
        -- ##########################

        -- THIS CURRENTLY *WILL* BE LOGGED THUS WE SKIP CHECK: ('create user if not exists u_gh_8062_existent password ''123'' using plugin Srp; -- must NOT be logged because already exists (current DB is SELF-SECURITY!)')
        -- ('create user if not exists u_gh_8062_missed password ''123'' using plugin Srp; -- MUST be logged because not yet exists')

        set bulk_insert insert into es_list(sttm) values(?);
        ('create mapping if not exists map_local_existent using plugin Srp from any user to user; -- must NOT be logged because already exists (current DB is SELF-SECURITY!)')
        ('create mapping if not exists map_local_missed using plugin Srp from any user to user; -- MUST be logged because not yet exists')
        ('create global mapping if not exists map_global_existent using plugin Srp from any user to user; -- must NOT be logged because already exists')
        ('create global mapping if not exists map_global_missed using plugin Srp from any user to user;')
        ('create role if not exists role_existent; -- must NOT be logged because already exists')
        ('create role if not exists role_missed; -- MUST be logged because not yet exists')
        ('create domain if not exists dm_existent as int; -- must NOT be logged because already exists')
        ('create domain if not exists dm_missed as int; -- MUST be logged because not yet exists')
        ('create sequence if not exists gen_existent; -- must NOT be logged because already exists')
        ('create sequence if not exists gen_missed; -- MUST be logged because not yet exists')
        ('create exception if not exists exc_existent ''bar''; -- must NOT be logged because already exists')
        ('create exception if not exists exc_missed ''rio''; -- MUST be logged because not yet exists')
        ('create collation if not exists coll_existent for iso8859_1 from pt_pt; -- must NOT be logged because already exists')
        ('create collation if not exists coll_missed for iso8859_1 from pt_pt; -- MUST be logged because not yet exists')
        ('create index if not exists t_existent_f01 on t_existent(f01_existent); -- must NOT belogged because such index already exists')
        ('create descending index if not exists t_missed_f01 on t_existent(f01_existent); -- MUST be logged: this index not yet exists')
        ('create view if not exists v_existent as select * from t_existent; -- must NOT be logged: this view already exists')
        ('create view if not exists v_missed as select * from t_existent; -- MUST be logged: this view not yet exists')
        ('create trigger if not exists trg_existent for t_existent after insert or update or delete as begin end;')
        ('create trigger if not exists trg_missed for t_existent after insert or update or delete as begin end;')
        ('create procedure if not exists sp_existent as begin end; -- must NOT be logged because already exists')
        ('create procedure if not exists sp_missed as begin end; -- MUST be logged because not yet exists')
        ('create function if not exists fn_existent(a_1 bigint) returns int128 as
        begin

            return a_1 * a_1;
        end -- must NOT be logged: this function already exists (and has different signature)')
        ('create function if not exists fn_missed(a_1 bigint) returns int128 as
        begin
            return a_1 * a_1;
        end -- MUST be logged because such function not yet exists')
        ('create package body if not exists pg_existent as
        begin
            procedure p_diff as
            begin
            end
            function f_diff returns int as
            begin
                return 1;
            end
        end -- must NOT be logged: such package already exists (and has different names of its units)')
        ('create package body if not exists pg_missed_implementation as
        begin
            procedure p_missed as
            begin
            end
            function f_missed returns int as
            begin
                return 1;
            end
        end -- MUST be logged because package exists but its BODY was not yet created')
        ('alter table t_existent add if not exists f01_existent smallint; -- must NOT be logged because column f01_existent DOES exist')
        ('alter table t_existent add if not exists g01_missed smallint; -- must be logged because column g01_missed NOT YET exist')
        ('alter table t_one_else_existent
            -- doc:
            -- "For ALTER TABLE ... ADD subclause, DDL triggers are not fired if there are only IF NOT EXISTS subclauses
            -- and ALL of them are related to EXISTING columns or constraints."
            ------------------
            -- must be logged because at least one column (g01_one_else_missed) NOT YET exist:
             add if not exists g01_one_else_missed decfloat(34)
            ,add if not exists f01_one_else_existent smallint
            ,add if not exists f02_one_else_existent int128')
        ('create table if not exists t_existent (id int primary key); -- must NOT be logged because such table altready exists')
        ('create table if not exists t_missed (id int primary key); -- MUST be logged because such table not yet exists')
        ('create procedure if not exists t_missed as
        begin
            -- must NOT be logged, procedure must NOT be created and NO error must raise.
            -- doc:
            -- Some objects share the same "namespace", for example, there cannot be a table and a procedure with the same name.
            -- In this case, if there is table T_MISSED and CREATE PROCEDURE IF NOT EXISTS T_MISSED is tried, the procedure
            -- will not be created and no error will be raised.
        end')
        stop

        commit;

        set term ^;
        execute block as
        begin
            for
                select sttm from es_list as cursor c
            do begin
                 rdb$set_context('USER_SESSION', 'RUNNING_DDL', c.sttm);
                execute statement c.sttm
                with autonomous transaction;
            end
        end
        ^
        set term ;^

        commit;

        -- ###################################
        -- CHECK RESULT: SHOW DDL TRIGGERS LOG 
        -- ###################################
        -- ::: NB :::
        -- ############################################################
        -- Use 'select * from v_detailed_ddl_log' if any problem occurs
        -- Query to this view will display executed DDL statements
        -- and comments about whether their must [not] be logged.
        -- ############################################################
        set count on;
        select * from v_check_ddl_log;
        rollback;
        /*
        connect '{REQUIRED_ALIAS}' user {act.db.user};
        drop database;
        quit;
        -- */
    """

    expected_stdout = f"""
        MON$SEC_DATABASE Self

        ID 1
        DDL_TRIGGER_NAME CREATE_MAPPING_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE MAPPING
        DDL_EVENT CREATE MAPPING
        OBJECT_NAME MAP_LOCAL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 2
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE MAPPING
        DDL_EVENT CREATE MAPPING
        OBJECT_NAME MAP_LOCAL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 3
        DDL_TRIGGER_NAME CREATE_MAPPING_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE MAPPING
        DDL_EVENT CREATE MAPPING
        OBJECT_NAME MAP_LOCAL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 4
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE MAPPING
        DDL_EVENT CREATE MAPPING
        OBJECT_NAME MAP_LOCAL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 5
        DDL_TRIGGER_NAME CREATE_MAPPING_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE MAPPING
        DDL_EVENT CREATE MAPPING
        OBJECT_NAME MAP_GLOBAL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 6
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE MAPPING
        DDL_EVENT CREATE MAPPING
        OBJECT_NAME MAP_GLOBAL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 7
        DDL_TRIGGER_NAME CREATE_MAPPING_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE MAPPING
        DDL_EVENT CREATE MAPPING
        OBJECT_NAME MAP_GLOBAL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 8
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE MAPPING
        DDL_EVENT CREATE MAPPING
        OBJECT_NAME MAP_GLOBAL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 9
        DDL_TRIGGER_NAME CREATE_ROLE_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE ROLE
        DDL_EVENT CREATE ROLE
        OBJECT_NAME ROLE_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 10
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE ROLE
        DDL_EVENT CREATE ROLE
        OBJECT_NAME ROLE_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 11
        DDL_TRIGGER_NAME CREATE_ROLE_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE ROLE
        DDL_EVENT CREATE ROLE
        OBJECT_NAME ROLE_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 12
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE ROLE
        DDL_EVENT CREATE ROLE
        OBJECT_NAME ROLE_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 13
        DDL_TRIGGER_NAME CREATE_DOMAIN_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE DOMAIN
        DDL_EVENT CREATE DOMAIN
        OBJECT_NAME DM_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 14
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE DOMAIN
        DDL_EVENT CREATE DOMAIN
        OBJECT_NAME DM_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 15
        DDL_TRIGGER_NAME CREATE_DOMAIN_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE DOMAIN
        DDL_EVENT CREATE DOMAIN
        OBJECT_NAME DM_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 16
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE DOMAIN
        DDL_EVENT CREATE DOMAIN
        OBJECT_NAME DM_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 17
        DDL_TRIGGER_NAME CREATE_SEQUENCE_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE SEQUENCE
        DDL_EVENT CREATE SEQUENCE
        OBJECT_NAME GEN_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 18
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE SEQUENCE
        DDL_EVENT CREATE SEQUENCE
        OBJECT_NAME GEN_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 19
        DDL_TRIGGER_NAME CREATE_SEQUENCE_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE SEQUENCE
        DDL_EVENT CREATE SEQUENCE
        OBJECT_NAME GEN_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 20
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE SEQUENCE
        DDL_EVENT CREATE SEQUENCE
        OBJECT_NAME GEN_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 21
        DDL_TRIGGER_NAME CREATE_EXCEPTION_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE EXCEPTION
        DDL_EVENT CREATE EXCEPTION
        OBJECT_NAME EXC_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 22
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE EXCEPTION
        DDL_EVENT CREATE EXCEPTION
        OBJECT_NAME EXC_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 23
        DDL_TRIGGER_NAME CREATE_EXCEPTION_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE EXCEPTION
        DDL_EVENT CREATE EXCEPTION
        OBJECT_NAME EXC_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 24
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE EXCEPTION
        DDL_EVENT CREATE EXCEPTION
        OBJECT_NAME EXC_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 25
        DDL_TRIGGER_NAME CREATE_COLLATION_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE COLLATION
        DDL_EVENT CREATE COLLATION
        OBJECT_NAME COLL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 26
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE COLLATION
        DDL_EVENT CREATE COLLATION
        OBJECT_NAME COLL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 27
        DDL_TRIGGER_NAME CREATE_COLLATION_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE COLLATION
        DDL_EVENT CREATE COLLATION
        OBJECT_NAME COLL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 28
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE COLLATION
        DDL_EVENT CREATE COLLATION
        OBJECT_NAME COLL_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 29
        DDL_TRIGGER_NAME CREATE_INDEX_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE INDEX
        DDL_EVENT CREATE INDEX
        OBJECT_NAME T_MISSED_F01
        FIRED_DDL_TRG_COUNT 4
        ID 30
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE INDEX
        DDL_EVENT CREATE INDEX
        OBJECT_NAME T_MISSED_F01
        FIRED_DDL_TRG_COUNT 4
        ID 31
        DDL_TRIGGER_NAME CREATE_INDEX_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE INDEX
        DDL_EVENT CREATE INDEX
        OBJECT_NAME T_MISSED_F01
        FIRED_DDL_TRG_COUNT 4
        ID 32
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE INDEX
        DDL_EVENT CREATE INDEX
        OBJECT_NAME T_MISSED_F01
        FIRED_DDL_TRG_COUNT 4
        ID 33
        DDL_TRIGGER_NAME CREATE_VIEW_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE VIEW
        DDL_EVENT CREATE VIEW
        OBJECT_NAME V_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 34
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE VIEW
        DDL_EVENT CREATE VIEW
        OBJECT_NAME V_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 35
        DDL_TRIGGER_NAME CREATE_VIEW_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE VIEW
        DDL_EVENT CREATE VIEW
        OBJECT_NAME V_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 36
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE VIEW
        DDL_EVENT CREATE VIEW
        OBJECT_NAME V_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 37
        DDL_TRIGGER_NAME CREATE_TRIGGER_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE TRIGGER
        DDL_EVENT CREATE TRIGGER
        OBJECT_NAME TRG_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 38
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE TRIGGER
        DDL_EVENT CREATE TRIGGER
        OBJECT_NAME TRG_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 39
        DDL_TRIGGER_NAME CREATE_TRIGGER_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE TRIGGER
        DDL_EVENT CREATE TRIGGER
        OBJECT_NAME TRG_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 40
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE TRIGGER
        DDL_EVENT CREATE TRIGGER
        OBJECT_NAME TRG_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 41
        DDL_TRIGGER_NAME CREATE_PROCEDURE_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE PROCEDURE
        DDL_EVENT CREATE PROCEDURE
        OBJECT_NAME SP_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 42
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE PROCEDURE
        DDL_EVENT CREATE PROCEDURE
        OBJECT_NAME SP_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 43
        DDL_TRIGGER_NAME CREATE_PROCEDURE_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE PROCEDURE
        DDL_EVENT CREATE PROCEDURE
        OBJECT_NAME SP_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 44
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE PROCEDURE
        DDL_EVENT CREATE PROCEDURE
        OBJECT_NAME SP_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 45
        DDL_TRIGGER_NAME CREATE_FUNCTION_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE FUNCTION
        DDL_EVENT CREATE FUNCTION
        OBJECT_NAME FN_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 46
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE FUNCTION
        DDL_EVENT CREATE FUNCTION
        OBJECT_NAME FN_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 47
        DDL_TRIGGER_NAME CREATE_FUNCTION_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE FUNCTION
        DDL_EVENT CREATE FUNCTION
        OBJECT_NAME FN_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 48
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE FUNCTION
        DDL_EVENT CREATE FUNCTION
        OBJECT_NAME FN_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 49
        DDL_TRIGGER_NAME CREATE_PACKAGE_BODY_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE PACKAGE BODY
        DDL_EVENT CREATE PACKAGE BODY
        OBJECT_NAME PG_MISSED_IMPLEMENTATION
        FIRED_DDL_TRG_COUNT 4
        ID 50
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE PACKAGE BODY
        DDL_EVENT CREATE PACKAGE BODY
        OBJECT_NAME PG_MISSED_IMPLEMENTATION
        FIRED_DDL_TRG_COUNT 4
        ID 51
        DDL_TRIGGER_NAME CREATE_PACKAGE_BODY_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE PACKAGE BODY
        DDL_EVENT CREATE PACKAGE BODY
        OBJECT_NAME PG_MISSED_IMPLEMENTATION
        FIRED_DDL_TRG_COUNT 4
        ID 52
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE PACKAGE BODY
        DDL_EVENT CREATE PACKAGE BODY
        OBJECT_NAME PG_MISSED_IMPLEMENTATION
        FIRED_DDL_TRG_COUNT 4
        ID 53
        DDL_TRIGGER_NAME ALTER_TABLE_BEFORE
        EVENT_TYPE ALTER
        OBJECT_TYPE TABLE
        DDL_EVENT ALTER TABLE
        OBJECT_NAME T_EXISTENT
        FIRED_DDL_TRG_COUNT 4
        ID 54
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE ALTER
        OBJECT_TYPE TABLE
        DDL_EVENT ALTER TABLE
        OBJECT_NAME T_EXISTENT
        FIRED_DDL_TRG_COUNT 4
        ID 55
        DDL_TRIGGER_NAME ALTER_TABLE_AFTER
        EVENT_TYPE ALTER
        OBJECT_TYPE TABLE
        DDL_EVENT ALTER TABLE
        OBJECT_NAME T_EXISTENT
        FIRED_DDL_TRG_COUNT 4
        ID 56
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE ALTER
        OBJECT_TYPE TABLE
        DDL_EVENT ALTER TABLE
        OBJECT_NAME T_EXISTENT
        FIRED_DDL_TRG_COUNT 4
        ID 57
        DDL_TRIGGER_NAME ALTER_TABLE_BEFORE
        EVENT_TYPE ALTER
        OBJECT_TYPE TABLE
        DDL_EVENT ALTER TABLE
        OBJECT_NAME T_ONE_ELSE_EXISTENT
        FIRED_DDL_TRG_COUNT 4
        ID 58
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE ALTER
        OBJECT_TYPE TABLE
        DDL_EVENT ALTER TABLE
        OBJECT_NAME T_ONE_ELSE_EXISTENT
        FIRED_DDL_TRG_COUNT 4
        ID 59
        DDL_TRIGGER_NAME ALTER_TABLE_AFTER
        EVENT_TYPE ALTER
        OBJECT_TYPE TABLE
        DDL_EVENT ALTER TABLE
        OBJECT_NAME T_ONE_ELSE_EXISTENT
        FIRED_DDL_TRG_COUNT 4
        ID 60
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE ALTER
        OBJECT_TYPE TABLE
        DDL_EVENT ALTER TABLE
        OBJECT_NAME T_ONE_ELSE_EXISTENT
        FIRED_DDL_TRG_COUNT 4
        ID 61
        DDL_TRIGGER_NAME CREATE_TABLE_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE TABLE
        DDL_EVENT CREATE TABLE
        OBJECT_NAME T_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 62
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_BEFORE
        EVENT_TYPE CREATE
        OBJECT_TYPE TABLE
        DDL_EVENT CREATE TABLE
        OBJECT_NAME T_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 63
        DDL_TRIGGER_NAME CREATE_TABLE_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE TABLE
        DDL_EVENT CREATE TABLE
        OBJECT_NAME T_MISSED
        FIRED_DDL_TRG_COUNT 4
        ID 64
        DDL_TRIGGER_NAME ANY_DDL_STATEMENT_AFTER
        EVENT_TYPE CREATE
        OBJECT_TYPE TABLE
        DDL_EVENT CREATE TABLE
        OBJECT_NAME T_MISSED
        FIRED_DDL_TRG_COUNT 4

        Records affected: 64
    """
    
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q', act.db.db_path, '-user', act.db.user], input = check_sql, credentials = False, connect_db = False, combine_output = True, io_enc = locale.getpreferredencoding())

    # temply, use 4debug when DB is not self-sec:
    #act.isql(input = check_sql, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
