    -- connect '%(dsn)s' user %(user_name)s password '%(user_password)s';
    set list on;
    set count on;

    set term ^;
    execute block as
    begin
       begin
           execute statement 'drop domain dm_test';
           when any do begin end
       end
    end
    ^
    set term ;^
    commit;

    -- ALTER statement will try to change its NOT_NULL state:
    create domain dm_test as int;
    commit;

    recreate table test( uid char(16) character set octets, x int, y int, constraint test_unq unique(uid) );
    commit;

    comment on table test is 'This is table TEST. And no one allowed to alter it!.';
    commit;

    create descending index test_uid on test(uid); -- ALTER statement will try to change its state to INACTIVE
    commit;

    set term ^;
    create or alter trigger test_bi for test active before insert position 0 as
    begin
       new.uid = gen_uuid();
    end
    ^
    set term ;^
    commit;

    alter user tmp$c6078_0 using plugin Srp revoke admin role;   -- Unprivileged user
    -- create or alter user tmp$c6078_0 password '123' using plugin Srp revoke admin role; -- Unprivileged user
    -- create or alter user tmp$c6078_1 password '123' using plugin Srp;
    -- create or alter user tmp$c6078_2 password '456' using plugin Srp;
    -- commit;

    create or alter mapping local_map_c6078 using plugin srp from user tmp$c6078_1 to user ltost;
    create or alter global mapping global_map_c6078 using plugin srp from user tmp$c6078_2 to user gtost;
    commit;

    connect '%(dsn)s' user tmp$c6078_0 password '123';
    commit;

    -- ######################################################################################################
    -- ###    F o l l o w i n g    i s     d o n e     b y    n o n - p r i v i l e g e d      u s e r    ###
    -- ######################################################################################################


    -- 29.01.2020. Attempt to alter another non-privileged USER.
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- modify record error
    -- -no permission for UPDATE access to COLUMN PLG$SRP_VIEW.PLG$ACTIVE
    -- (FB 4.0.0 only): -Effective user is TMP$C6078_0
    alter user tmp$c6078_1 inactive using plugin Srp;
    commit;

    -- 29.01.2020. Attempt to alter THE WHOLE DATABASE.
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -ALTER DATABASE failed
    -- -no permission for ALTER access to DATABASE
    alter database set linger to 31;
    commit;

    ----------------------------------------------------------------

    -- 29.01.2020. Attempt to alter DOMAIN.
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -ALTER DOMAIN DM_TEST failed
    -- -no permission for ALTER access to DOMAIN DM_TEST
    -- (FB 4.0.0 only): -Effective user is TMP$C6078_0
    alter domain dm_test set not null;
    commit;

    ----------------------------------------------------------------

    -- 29.01.2020. Attempt to alter table DROP constraint.
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -ALTER TABLE TEST failed
    -- -no permission for ALTER access to TABLE TEST
    -- (FB 4.0.0 only): -Effective user is TMP$C6078_0
    alter table test drop constraint test_unq;

    ----------------------------------------------------------------

    -- 29.01.2020. Attempt to alter table alter column.
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -ALTER TABLE TEST failed
    -- -no permission for ALTER access to TABLE TEST
    -- (FB 4.0.0 only): -Effective user is TMP$C6078_0
    alter table test alter x type bigint;

    ----------------------------------------------------------------

    -- 29.01.2020. Attempt to alter INDEX: make it inactive.
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -ALTER INDEX TEST_UID failed
    -- -no permission for ALTER access to TABLE TEST
    -- -Effective user is TMP$C6078_0
    alter index test_uid inactive;

    ----------------------------------------------------------------

    -- 29.01.2020. Attempt to change existing COMMENT to the table TEST (make it NULL).
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -COMMENT ON TEST failed
    -- -no permission for ALTER access to TABLE TEST
    -- (FB 4.0.0): -Effective user is TMP$C6078_0
    comment on table test is null;

    ----------------------------------------------------------------

    -- Attempt to alter TRIGGER on existing table (CREATED BY SYSDBA)
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -CREATE OR ALTER TRIGGER TEST_BI failed
    -- -no permission for ALTER access to TABLE TEST
    -- (FB 4.0.0 only): -Effective user is TMP$C6078_0
    set term ^;
    create or alter trigger test_bi for test active before insert position 0 as
    begin
       new.uid = 'QWE';
    end
    ^
    set term ;^
    commit;

    ----------------------------------------------------------------

    -- Attempt to create/alter TRIGGER on DB-level event:
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -CREATE OR ALTER TRIGGER TRG$START failed
    -- -no permission for ALTER access to DATABASE
    set term ^;
    create or alter trigger trg$start
        inactive on transaction start position 0
    as
    begin
        rdb$set_context('USER_SESSION', 'TRANS_ID', current_transaction);
    end
    ^
    set term ;^

    ----------------------------------------------------------------

    -- Attempt to alter TRIGGER for DDL event:
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -CREATE OR ALTER TRIGGER TRIG_DDL_SP failed
    -- -no permission for ALTER access to DATABASE
    set term ^;
    create or alter trigger trig_ddl_sp before create procedure as
    begin
    end
    ^
    set term ;^


    -- Check that there is still ONE trigger that was created at the start ofthis script (by SYSDBA) and it has unchanged body:
    -- Expected:
    -- RDB$TRIGGER_NAME                TEST_BI
    -- RDB$TRIGGER_SOURCE              c:3cc
    -- as
    -- begin
    --    new.uid = gen_uuid();
    -- end
    -- Records affected: 1

    select t.rdb$trigger_name altered_trigger_name, t.rdb$trigger_source altered_trigger_source
    from rdb$database r
    left join rdb$triggers t on t.rdb$system_flag is distinct from 1;

    ---------------------------------------------------------------

    -- Attempt to alter PACKAGE header.
    -- Expected:
    -- Statement failed, SQLSTATE = 42000
    -- unsuccessful metadata update
    -- -CREATE OR ALTER PACKAGE PKG_TEST failed
    -- -No permission for CREATE PACKAGE operation
    set term ^;
    create or alter package pkg_test -- error did raise, but packages still WAS created.
    as
    begin
      function f_test_inside_pkg
      returns smallint;
    end
    ^
    set term ;^

    ---------------------------------------------------------------

    -- Attempt to alter PACKAGE body.
    -- Expected:
    -- Statement failed, SQLSTATE = 42000
    -- unsuccessful metadata update
    -- -RECREATE PACKAGE BODY PKG_TEST failed
    -- -No permission for CREATE PACKAGE operation
    set term ^;
    recreate package body PKG_TEST
    as
    begin
      function f_test_inside_pkg
      returns smallint
      as
      begin
        return 1;
      end
    end
    ^
    set term ;^

    commit;

    -- Check that no packages did appear in the database.
    -- Expected:
    -- RDB$PACKAGE_NAME                <null>
    -- Records affected: 1
    select p.rdb$package_name as altered_pkg_name
    from rdb$database r
    left join rdb$packages p on p.rdb$system_flag is distinct from 1;
    commit;


    ---------------------------------------------------------------

    -- Attempt to alter standalone PSQL function
    set term ^;
    create or alter function fn_c6078 returns int as  -- error did raise, but function still WAS created.
    begin
      return 123987;
    end
    ^
    set term ;^
    commit;

    -- Expected:
    -- RDB$FUNCTION_NAME               <null>
    -- Records affected: 1
    select f.rdb$function_name as altered_standalone_func from rdb$database r left join rdb$functions f on f.rdb$system_flag is distinct from 1 and f.RDB$PACKAGE_NAME is null;
    commit;

    ---------------------------------------------------------------

    -- Attempt to alter standalone procedure
    set term ^;
    create or alter procedure sp_c6078 returns(whoami varchar(32)) as
    begin
        whoami = current_user;
        suspend;
    end
    ^
    set term ;^
    commit;

    -- Expected:
    -- RDB$PROCEDURE_NAME              <null>
    -- Records affected: 1
    select p.rdb$procedure_name as altered_standalone_proc from rdb$database r left join rdb$procedures p on p.rdb$system_flag is distinct from 1;

    ---------------------------------------------------------------

    --  Attempt to alter view
    create or alter view v_c6078 as select * from rdb$database; -- NO error at all, view WAS created.
    commit;

    -- Expected
    -- RDB$RELATION_NAME               <null>
    -- Records affected: 1
    select v.rdb$relation_name as altered_view_name from rdb$database r left join rdb$relations v on v.rdb$system_flag is distinct from 1 and v.rdb$relation_name = upper('v_c6078');
    commit;

    ---------------------------------------------------------------
    --  Attempt to alter sequence

    create or alter sequence sq_c6078 start with 192837465;
    commit;

    -- Expected:
    -- RDB$GENERATOR_NAME              <null>
    -- Records affected: 1
    select g.rdb$generator_name as altered_sequence_name from rdb$database r left join rdb$generators g on g.rdb$system_flag is distinct from 1;
    commit;

    ---------------------------------------------------------------
    --  Attempt to alter exception

    --create or alter exception ex_c6078 'Something wrong.'; -- here no error, even for 1st run of this statement!
    create or alter exception ex_c6078 'Something wrong.';
    commit;

    -- Expected
    -- RDB$EXCEPTION_NAME              <null>
    -- Records affected: 1
    select x.rdb$exception_name as altered_exception_name from rdb$database r left join rdb$exceptions x on x.rdb$system_flag is distinct from 1;
    commit;

    ---------------------------------------------------------------

    -- Attempt to alter UDR-base function:
    -- before fix there was NO error here and UDR-based funtion WAS created
    create or alter function wait_event (
        event_name varchar(31) character set utf8 not null
    ) returns integer not null
        external name 'udrcpp_example!wait_event'
        engine udr;

    commit;

    -- Expected:
    -- RDB$FUNCTION_NAME               <null>
    -- Records affected: 1
    select f.rdb$function_name as altered_UDR_based_func from rdb$database r left join rdb$functions f on f.rdb$system_flag is distinct from 1 and f.rdb$engine_name = upper('udr');
    commit;

    ---------------------------------------------------------------

    -- 29.01.2020. Attempt to alter character set.
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -ALTER CHARACTER SET UTF8 failed
    -- -no permission for ALTER access to CHARACTER SET UTF8
    -- (FB 4.0.0 only): -Effective user is TMP$C6078_0
    ALTER CHARACTER SET UTF8 SET DEFAULT COLLATION UNICODE_CI_AI;

    ---------------------------------------------------------------

    -- 29.01.2020. Attempt to alter LOCAL MAPPING.
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -ALTER MAPPING LOCAL_MAP_C6078 failed
    -- -Unable to perform operation
    -- 4.0.0: -System privilege CHANGE_MAPPING_RULES is missing
    -- 3.0.x: -Unable to perform operation.  You must be either SYSDBA or owner of the database
    alter mapping local_map_c6078 using plugin srp from user tmp$c6078_1 to user ltost_2;

    ---------------------------------------------------------------

    -- 29.01.2020. Attempt to alter GLOBAL MAPPING.
    -- Expected:
    -- Statement failed, SQLSTATE = 28000
    -- unsuccessful metadata update
    -- -ALTER MAPPING GLOBAL_MAP_C6078 failed
    -- -Unable to perform operation
    -- (FB 4.0.0): -System privilege CHANGE_MAPPING_RULES is missing
    -- (FB 3.0.x): -Unable to perform operation.  You must be either SYSDBA or owner of the database
    alter global mapping global_map_c6078 using plugin srp from user tmp$c6078_2 to user gtost_2;
    commit;

    -- cleanup:
    -- ########
    connect '%(dsn)s' user %(user_name)s password '%(user_password)s';

    drop global mapping global_map_c6078;
    drop mapping local_map_c6078;
    commit;

    -- drop user tmp$c6078_0 using plugin Srp;
    -- drop user tmp$c6078_1 using plugin Srp;
    -- drop user tmp$c6078_2 using plugin Srp;
    -- commit;
