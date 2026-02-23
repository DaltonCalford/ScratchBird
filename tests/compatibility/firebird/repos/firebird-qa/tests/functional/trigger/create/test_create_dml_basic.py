#coding:utf-8

"""
ID:          n/a
TITLE:       CREATE TRIGGER - basic checks for DML triggers
DESCRIPTION:
    Test checks several scnarios with 'CREATE TRIGGER' statement, they are titled in 'msg_map' dict.
    Statements can cause either successful outcome or raise exception because of some rule(s) violation.
    We check content of RDB$ tables in order to see data for triggers(s) INSTEAD of usage 'SHOW DOMAIN' command.
    View 'v_trig_info' is used to show all data related to domains.
    Its DDL differs for FB versions prior/ since 6.x (columns related to SQL schemas present for 6.x).
NOTES:
    [12.07.2025] pzotov
    1. This test replaces previously created ones with names:
          test_01.py test_05.py test_09.py
          test_02.py test_06.py test_10.py
          test_03.py test_07.py test_17.py
          test_04.py test_08.py
       All these tests has been marked to be SKIPPED from execution.
    2. ::: NB ::: Reconnect must be done after failed attempt to create `AFTER` trigger that changes `new.` variable.
       See here: https://github.com/FirebirdSQL/firebird/issues/1833#issuecomment-3067022194
    3. Trigger that uses UDR or SQL SECURITY is checked on 4.x+

    Checked on 6.0.0.970; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import locale
import pytest
from firebird.qa import *

db = db_factory()
tmp_junior = user_factory('db', name = 'tmp_junior', password = '123')

substitutions = [
                     ('AUDIT_ID\\s+\\d+', '')
                    ,('AUDIT_TS\\s+\\d{4}.*', '')
                    ,('AUDIT_TX\\s+\\d+', '')
                    ,("(-)?At trigger \\S+ line:\\s?\\d+.*", '')
                    ,('(-)?At line(:)?\\s+\\d+(,)?\\s+col(umn)?(:)?\\s+\\d+', '')
                ]

act = isql_act('db', substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_junior: User):

    TRG_SCHEMA_COLUMN = '' if act.is_version('<6') else ',rdb$schema_name as trg_schema'
    TRG_SCHEMA_FLDVAL = '' if act.is_version('<6') else 'TRG_SCHEMA                      PUBLIC'
    READ_ONLY_COLUMN = 'TEST.ID' if act.is_version('<6') else '"PUBLIC"."TEST"."ID"'

    msg_map = {
        'test_01' : "Trigger BEFORE INSERT"
       ,'test_02' : "Trigger AFTER INSERT"
       ,'test_03' : "Trigger BEFORE UPDATE"
       ,'test_04' : "Trigger AFTER UPDATE"
       ,'test_05' : "Trigger BEFORE DELETE"
       ,'test_06' : "Trigger AFTER DELETE"
       ,'test_07' : "Trigger in INACTIVE state"
       ,'test_08' : "Trigger with specifying its POSITION. Check result for several triggers"
       ,'test_09' : "Trigger for a VIEW"
       ,'test_10' : "Trigger with exception"
       ,'test_11' : "Trigger with post event"
       ,'test_12' : "Trigger that changes a table for which it was created (recursion)"
       ,'test_13' : "Attempt to create `AFTER` trigger must fail if `new.` is changed"
       ,'test_14' : "Attempt to create `ON INSERT` trigger must fail if `old.` presents in its source"
       ,'test_15' : "Attempt to create `ON DELETE` trigger must fail if `new.` presents in its source"
       ,'test_16' : "Trigger that calls routine based on external engine (UDR)"
       ,'test_17' : "Trigger with SQL SECURITY clause"
    }
    for k,v in msg_map.items():
        msg_map[k] = '. '.join( (k,v) )

    VERIFY_STATEMENTS = """
        set count on;
        select
            audit_id
           ,audit_ts
           ,audit_tx
           ,old_id
           ,old_f01
           ,new_id
           ,new_f01
           ,dml_info
        from taud
        order by audit_id;
        set count off;
        select v.* from v_trig_info v;
        select t.* from v_timestamps_info t;
        commit;
    """

    CLEANUP_STATEMENTS = """
        drop trigger trg_test;
        delete from taud;
        delete from test;
        set term ^;
        execute block as
        begin
           rdb$set_context('USER_SESSION', 'BEFORE_DML_START', null);
           rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', null);
        end ^
        set term ;^
        commit;
    """
    
    test_script = f"""
        set wng off;
        set list on;
        create view v_trig_info as
        select
            rdb$trigger_name       as trg_name
            ,rdb$relation_name     as rel_name
            ,rdb$trigger_sequence  as trg_seqn
            ,iif(rdb$trigger_inactive is not distinct from 1, 'INACTIVE', 'active') as trg_act
            ,decode(
                rdb$trigger_type
               ,   1, 'before insert'
               ,   2, 'after insert'
               ,   3, 'before update'
               ,   4, 'after update'
               ,   5, 'before delete'
               ,   6, 'after delete'
               ,  17, 'before insert or update'
               ,  18, 'after insert or update'
               ,  25, 'before insert or delete'
               ,  26, 'after insert or delete'
               ,  27, 'before update or delete'
               ,  28, 'after update or delete'
               , 113, 'before insert or update or delete'
               , 114, 'after insert or update or delete'
               ,8192, 'on connect'
               ,8193, 'on disconnect'
               ,8194, 'on transaction start'
               ,8195, 'on transaction commit'
               ,8196, 'on transaction rollback'
             ) as trg_type
            ,rdb$valid_blr         as trg_valid_blr
            -- ,rdb$trigger_source    as blob_id_trg_source
            -- ,rdb$trigger_blr
            -- ,rdb$description       as blob_id_trg_descr
            --,rdb$system_flag       as trg_sys_flag
            -- ,rdb$flags             as trg_flags
            -- ,rdb$debug_info
            ,rdb$engine_name       as trg_engine
            ,rdb$entrypoint        as trg_entry
            {TRG_SCHEMA_COLUMN}
        from rdb$triggers
        where rdb$trigger_name starting with 'TRG_'
        order by rdb$trigger_name
        ;

        create view v_timestamps_info as
        select
            iif(coalesce(interval_ms,0) between 0 and 100, timestamp '01.01.0001 00:00:00', ts_before_dml_start) as ts_before_dml_start
           ,iif(coalesce(interval_ms,0) between 0 and 100, timestamp '01.01.0001 00:00:00', ts_at_end_of_trigger) as ts_at_end_of_trigger
           ,iif(coalesce(interval_ms,0) between 0 and 100, 'OK', 'WEIRD INTERVAL between ts_before_dml_start and ts_at_end_of_trigger: ' || interval_ms) as interval_ms
        from (
            select
                ts_before_dml_start
               ,ts_at_end_of_trigger
               ,datediff(millisecond from ts_before_dml_start to ts_at_end_of_trigger) as interval_ms
            from (
               select 
                   cast(rdb$get_context('USER_SESSION', 'BEFORE_DML_START') as timestamp) as ts_before_dml_start
                  ,cast(rdb$get_context('USER_SESSION', 'AT_END_OF_TRIGGER') as timestamp) as ts_at_end_of_trigger
               from rdb$database
            )
        );
        commit;
        grant select on v_timestamps_info to public;

        create sequence g;
        create exception exc_test 'Something wrong occurs: @1';

        create table test(id int constraint test_pk primary key, f01 int);
        create table tctx(
             ts_before_dml_start timestamp
            ,ts_at_end_of_trigger timestamp
            ,interval_ms computed by ( datediff(millisecond from ts_before_dml_start to ts_at_end_of_trigger) )
        );

        -- for testing trigger for a VIEW that is based on these tables:
        create table test_a(id int constraint test_a_pk primary key, f01 int);
        create table test_b(id int constraint test_b_pk primary key, f01 int);
        create table test_c(id int constraint test_c_pk primary key, f01 int);

        create view v_test as
        select 'a'as t_source, id, f01 from test_a union all
        select 'b', id, f01 from test_b union all
        select 'c', id, f01 from test_c
        ;

        create table taud(
            audit_id int generated by default as identity constraint taud_pk primary key
           ,audit_ts timestamp default 'now'
           ,audit_tx int default current_transaction
           ,old_id int
           ,old_f01 int
           ,new_id int
           ,new_f01 int
           ,dml_info varchar(80)
        );
        commit;

        -- set echo on;
        set bail off;
        ----------------------------
        select '{msg_map["test_01"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test before insert as
        begin
            new.id = coalesce(new.id, gen_id(g,1));
            new.f01 = coalesce(new.id, 1);
            in autonomous transaction do
            insert into taud(new_id, new_f01, dml_info) values(new.id, new.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));
        end ^
        commit ^
        execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^
        set term ;^
        insert into test(id, f01) values(gen_id(g,1), 100);
        commit;
        {VERIFY_STATEMENTS}
        {CLEANUP_STATEMENTS}
        ----------------------------
        select '{msg_map["test_02"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test after insert as
        begin
            in autonomous transaction do
            insert into taud(new_id, new_f01, dml_info) values(new.id, new.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));
        end ^
        commit ^
        execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^
        set term ;^
        insert into test(id, f01) values(gen_id(g,1),200);
        commit;
        {VERIFY_STATEMENTS}
        {CLEANUP_STATEMENTS}
        ----------------------------
        select '{msg_map["test_03"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test before update as
        begin
            in autonomous transaction do
            insert into taud(old_id, old_f01, new_id, new_f01, dml_info) values(old.id, old.f01, new.id, new.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));
        end ^
        commit ^
        execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^
        set term ;^
        insert into test(id, f01) values(-3,299);
        update test set id = 3, f01 = f01 + 1 where id = -3;
        commit;
        {VERIFY_STATEMENTS}
        {CLEANUP_STATEMENTS}
        ----------------------------
        select '{msg_map["test_04"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test after update as
        begin
            in autonomous transaction do
            insert into taud(old_id, old_f01, new_id, new_f01, dml_info) values(old.id, old.f01, new.id, new.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));
        end ^
        commit ^
        insert into test(id, f01) values(-4,399) ^
        execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^
        set term ;^
        update test set id = 4, f01 = f01 + 1 where id = -4;
        commit;
        {VERIFY_STATEMENTS}
        {CLEANUP_STATEMENTS}
        ----------------------------
        select '{msg_map["test_05"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test before delete as
        begin
            in autonomous transaction do
            insert into taud(old_id, old_f01, dml_info) values(old.id, old.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));
        end ^
        commit ^
        insert into test(id, f01) values(-5,499) ^
        execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^
        set term ;^
        delete from test where id = -5;
        commit;
        {VERIFY_STATEMENTS}
        {CLEANUP_STATEMENTS}
        ----------------------------
        select '{msg_map["test_06"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test after delete as
        begin
            in autonomous transaction do
            insert into taud(old_id, old_f01, dml_info) values(old.id, old.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));
        end ^
        commit ^
        insert into test(id, f01) values(-6,599) ^
        execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^
        set term ;^
        delete from test where id = -6;
        commit;
        {VERIFY_STATEMENTS}
        {CLEANUP_STATEMENTS}
        ----------------------------
        -- Trigger in INACTIVE state
        select '{msg_map["test_07"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test INACTIVE after delete as
        begin
            in autonomous transaction do
            insert into taud(old_id, old_f01, dml_info) values(old.id, old.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));
        end ^
        commit ^
        insert into test(id, f01) values(-7,699) ^
        execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^
        set term ;^
        delete from test where id = -7;
        commit;
        {VERIFY_STATEMENTS}
        {CLEANUP_STATEMENTS}
        ----------------------------
        -- Trigger with specifying its POSITION. Check result for several triggers
        select '{msg_map["test_08"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test_a for test after delete position 2 as
        begin
            in autonomous transaction do
            insert into taud(old_id, old_f01, dml_info) values(old.id, old.f01, iif(inserting, 'ins_pos_2', iif(updating, 'upd_pos_2', 'del_pos_2')) );
        end ^
        create trigger trg_test_b for test after delete position 1 as
        begin
            in autonomous transaction do
            insert into taud(old_id, old_f01, dml_info) values(old.id, old.f01, iif(inserting, 'ins_pos_1', iif(updating, 'upd_pos_1', 'del_pos_1')) );
        end ^
        commit ^
        insert into test(id, f01) values(-8,799) ^
        set term ;^
        delete from test where id = -8;
        commit;
        {VERIFY_STATEMENTS}
        commit;
        drop trigger trg_test_a;
        drop trigger trg_test_b;
        delete from taud;
        delete from test;
        commit;
        ----------------------------
        -- Trigger for a VIEW
        select '{msg_map["test_09"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for v_test before insert or update or delete as
            declare v_target_table type of rdb$relation_name;
            declare v_dml_statement varchar(1024);
        begin
            if (inserting or updating) then
                begin
                    v_target_table = decode( mod(new.id, 3), 0, 'test_a', 1, 'test_b', 'test_c');
                    if (inserting) then
                        begin
                            v_dml_statement = 'insert into ' || v_target_table || '(id, f01) values(?, ?)';
                            execute statement (v_dml_statement) (new.id, new.f01);
                        end
                    else
                        begin
                            v_dml_statement = 'update ' || v_target_table || 'set f01 = ? where id = ?';
                            execute statement (v_dml_statement) (new.f01, old.id);
                        end
                end
            else
                begin
                    v_target_table = decode( mod(old.id, 3), 0, 'test_a', 1, 'test_b', 'test_c');
                    v_dml_statement = 'delete from ' || v_target_table || ' where id = ?';
                    execute statement (v_dml_statement) (old.id);
                end


            in autonomous transaction do
            begin
                if (inserting) then
                    begin
                        insert into taud(new_id, new_f01, dml_info) values(new.id, new.f01, 'ins:' || :v_target_table);
                    end
                else if (deleting) then
                    begin
                        insert into taud(old_id, old_f01, dml_info) values(old.id, old.f01, 'del:' || :v_target_table);
                    end
                else
                    begin
                        insert into taud(old_id, old_f01, new_id, new_f01, dml_info) values(old.id, old.f01, new.id, new.f01, 'upd:' || :v_target_table);
                    end
            end

            rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));

        end ^
        commit ^
        set term ;^
        insert into v_test(id, f01) values(0, 0);
        insert into v_test(id, f01) values(1, 1);
        insert into v_test(id, f01) values(2, 2);
        update v_test set f01 = f01 * f01 where id = 2;
        set term ^; execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^set term ;^
        delete from v_test where id = 0;
        commit;
        {VERIFY_STATEMENTS}
        set count on;
        select * from v_test;
        set count off;
        commit;
        {CLEANUP_STATEMENTS}
        drop view v_test;
        commit;
        ----------------------------
        -- Trigger with exception
        select '{msg_map["test_10"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test after delete as
        begin
            in autonomous transaction do
            insert into taud(old_id, old_f01, dml_info) values(old.id, old.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));

            if (old.id = -10) then
                exception exc_test using(old.id);
        end ^
        commit ^
        insert into test(id, f01) values(-10,999) ^
        execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^
        set term ;^
        delete from test where id = -10;
        commit;
        {VERIFY_STATEMENTS}
        {CLEANUP_STATEMENTS}
        ----------------------------
        -- Trigger with POST EVENT
        select '{msg_map["test_11"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test after delete as
        begin
            in autonomous transaction do
            insert into taud(old_id, old_f01, dml_info) values(old.id, old.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));

            if (old.id = -10) then
                POST_EVENT 'test';
        end ^
        commit ^
        insert into test(id, f01) values(-10,999) ^
        execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^
        set term ;^
        delete from test where id = -10;
        commit;
        {VERIFY_STATEMENTS}
        {CLEANUP_STATEMENTS}
        ----------------------------
        -- Trigger that changes a table for which it was created (recursion)
        select '{msg_map["test_12"]}' as msg from rdb$database;
        alter sequence g restart with 0;
        set term ^;
        create trigger trg_test for test before insert as
            declare v int;
        begin
            v = -gen_id(g,1);
            insert into test(id, f01) values(:v, :v);
            in autonomous transaction do
            insert into taud(new_id, new_f01, dml_info) values(new.id, new.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
        end ^
        commit ^
        set term ;^
        insert into test(id, f01) values(1,1);
        commit;
        select gen_id(g,0) as curr_gen from rdb$database;
        set count on;
        select * from test;
        set count off;
        {VERIFY_STATEMENTS}
        {CLEANUP_STATEMENTS}
        ----------------------------
        -- Attempt to create `AFTER` trigger must fail if `new.` is changed
        select '{msg_map["test_13"]}' as msg from rdb$database;
        set term ^;
        -- Statement failed, SQLSTATE = 42000
        -- attempted update of read-only column TEST.ID
        create trigger trg_test for test after insert or update as
        begin
            in autonomous transaction do
            begin
                new.id = rand() * 1000000;
                insert into taud(new_id, new_f01, dml_info) values(new.id, new.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            end
        end ^
        set term ;^
        commit;
        
        -- #####################################
        -- https://github.com/FirebirdSQL/firebird/issues/1833#issuecomment-3067022194
        connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';
        -- ######################################
        insert into test(id, f01) values(-13,-13); -- must PASS w/o errors, but currently reconnect is required!
        commit;
        select * from test;
        {VERIFY_STATEMENTS}
        set term ^;
        execute block returns(unexpected_alert_msg varchar(1024)) as
        begin
            if ( exists(select 1 from rdb$triggers where rdb$trigger_name = upper('trg_test')) ) then
            begin
                unexpected_alert_msg = '::: ACHTUNG ::: {msg_map["test_13"]} - RULE VIOLATED!';
                suspend;
            end
        end ^
        set term ;^
        commit;
        -- NB: attempt to drop trigger must fail because it should not be created:
        {CLEANUP_STATEMENTS}
        ----------------------------
        --  "Attempt to create `ON INSERT` trigger must fail if `old.` presents in its source"
        select '{msg_map["test_14"]}' as msg from rdb$database;
        set term ^;
        -- SQLSTATE = 42S22 / unsuccessful metadata update / ... / Column unknown OLD.ID
        create trigger trg_test for test after insert as
        begin
            in autonomous transaction do
            begin
                insert into taud(old_id, old_f01, dml_info) values(old.id, old.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            end
        end ^
        set term ;^
        commit;

        insert into test(id, f01) values(-14,-14); -- must PASS w/o errors
        commit;
        set count on;
        select * from test;
        set count off;
        {VERIFY_STATEMENTS}
        set term ^;
        execute block returns(unexpected_alert_msg varchar(1024)) as
        begin
            if ( exists(select 1 from rdb$triggers where rdb$trigger_name = upper('trg_test')) ) then
            begin
                unexpected_alert_msg = '::: ACHTUNG ::: {msg_map["test_14"]} - RULE VIOLATED!';
                suspend;
            end
        end ^
        set term ;^
        commit;
        -- NB: attempt to drop trigger must fail because it should not be created:
        {CLEANUP_STATEMENTS}
        ----------------------------
        -- Attempt to create `ON DELETE` trigger must fail if `new.` presents in its source
        select '{msg_map["test_15"]}' as msg from rdb$database;
        set term ^;
        -- SQLSTATE = 42S22 / unsuccessful metadata update / ... / Column unknown NEW.ID
        create trigger trg_test for test after delete as
        begin
            in autonomous transaction do
            begin
                insert into taud(new_id, new_f01, dml_info) values(new.id, new.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
            end
        end ^
        set term ;^
        commit;
        insert into test(id, f01) values(-15,-15);
        delete from test where id = -15;
        commit;
        set count on;
        select * from test;
        set count off;
        {VERIFY_STATEMENTS}
        set term ^;
        execute block returns(unexpected_alert_msg varchar(1024)) as
        begin
            if ( exists(select 1 from rdb$triggers where rdb$trigger_name = upper('trg_test')) ) then
            begin
                unexpected_alert_msg = '::: ACHTUNG ::: {msg_map["test_15"]} - RULE VIOLATED!';
                suspend;
            end
        end ^
        set term ;^
        commit;
        -- NB: attempt to drop trigger must fail because it should not be created:
        {CLEANUP_STATEMENTS}
    """

    if act.is_version('<4'):
        pass
    else:
        test_script += f"""
            ----------------------------
            -- Trigger that calls routine based on external engine (UDR)
            -- 3.x: SQLSTATE = HY000 / UDR module not loaded / <module not found> (localized)
            select '{msg_map["test_16"]}' as msg from rdb$database;

            create or alter function isLeapUDR (a_timestamp timestamp) returns boolean
            external name 'udf_compat!UC_isLeapYear'
            engine udr;
            commit;
            
            set term ^;
            create trigger trg_test for test before insert as
                declare v_date_for_id date;
            begin
                new.id = coalesce(new.id, gen_id(g,1));
                v_date_for_id = cast('01.01.' || new.id as date);
                new.f01 = iif(isLeapUDR(v_date_for_id), 1, 0);
                in autonomous transaction do
                insert into taud(new_id, new_f01, dml_info) values(new.id, new.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
                rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));
            end ^
            commit ^
            execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^
            set term ;^
            insert into test(id) values(2023);
            insert into test(id) values(2024);
            commit;
            {VERIFY_STATEMENTS}
            {CLEANUP_STATEMENTS}
            ----------------------------
            -- Trigger with SQL SECURITY clause, 4.x+
            select '{msg_map["test_17"]}' as msg from rdb$database;
            commit;
            
            revoke all on all from {tmp_junior.name};
            grant select, insert, update, delete on test to {tmp_junior.name};
            grant insert, delete on tctx to {tmp_junior.name};
            commit;

            set term ^;
            create trigger trg_test for test before insert
            SQL SECURITY DEFINER -- need if current user was not granted to INSERT into taud.
            as
            begin
                new.id = new.id + 0 * gen_id(g,1);
                new.f01 = coalesce(new.id, 1);
                in autonomous transaction do
                    insert into taud(new_id, new_f01, dml_info) values(new.id, new.f01, iif(inserting, 'ins', iif(updating, 'upd', 'del')) );
                    rdb$set_context('USER_SESSION', 'AT_END_OF_TRIGGER', cast('now' as timestamp));
            end ^
            commit ^
            set term ;^
            commit;

            connect '{act.db.dsn}' user {tmp_junior.name} password '{tmp_junior.password}';

            set term ^; execute block as begin rdb$set_context('USER_SESSION', 'BEFORE_DML_START', cast('now' as timestamp)); end ^ set term ;^
            insert into test(id, f01) values(1, 100);

            delete from tctx;
            insert into tctx(ts_before_dml_start, ts_at_end_of_trigger)
            select
                ts_before_dml_start
               ,ts_at_end_of_trigger
            from v_timestamps_info;
            commit;

            connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';

            set count on;
            select t.* from taud t;
            set count off;
            select v.* from v_trig_info v;

            select
                iif(coalesce(interval_ms,0) between 0 and 100, timestamp '01.01.0001 00:00:00', ts_before_dml_start) as ts_before_dml_start
               ,iif(coalesce(interval_ms,0) between 0 and 100, timestamp '01.01.0001 00:00:00', ts_at_end_of_trigger) as ts_at_end_of_trigger
               ,iif(coalesce(interval_ms,0) between 0 and 100, 'OK', 'WEIRD INTERVAL between ts_before_dml_start and ts_at_end_of_trigger: ' || interval_ms) as interval_ms
            from (
                select x.ts_before_dml_start, x.ts_at_end_of_trigger, x.interval_ms from tctx x
            );
            commit;

            {CLEANUP_STATEMENTS}
        """
       

    expected_stdout_3x = """
        MSG                             test_01. Trigger BEFORE INSERT
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          1
        NEW_F01                         1
        DML_INFO                        ins
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_02. Trigger AFTER INSERT
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          2
        NEW_F01                         200
        DML_INFO                        ins
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_03. Trigger BEFORE UPDATE
        OLD_ID                          -3
        OLD_F01                         299
        NEW_ID                          3
        NEW_F01                         300
        DML_INFO                        upd
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before update
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_04. Trigger AFTER UPDATE
        OLD_ID                          -4
        OLD_F01                         399
        NEW_ID                          4
        NEW_F01                         400
        DML_INFO                        upd
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after update
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_05. Trigger BEFORE DELETE
        OLD_ID                          -5
        OLD_F01                         499
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_06. Trigger AFTER DELETE
        OLD_ID                          -6
        OLD_F01                         599
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_07. Trigger in INACTIVE state
        Records affected: 0
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         INACTIVE
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_08. Trigger with specifying its POSITION. Check result for several triggers
        OLD_ID                          -8
        OLD_F01                         799
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del_pos_1
        OLD_ID                          -8
        OLD_F01                         799
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del_pos_2
        Records affected: 2
        TRG_NAME                        TRG_TEST_A
        REL_NAME                        TEST
        TRG_SEQN                        2
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_NAME                        TRG_TEST_B
        REL_NAME                        TEST
        TRG_SEQN                        1
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_09. Trigger for a VIEW
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          0
        NEW_F01                         0
        DML_INFO                        ins:test_a
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          1
        NEW_F01                         1
        DML_INFO                        ins:test_b
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          2
        NEW_F01                         2
        DML_INFO                        ins:test_c
        OLD_ID                          2
        OLD_F01                         2
        NEW_ID                          2
        NEW_F01                         4
        DML_INFO                        upd:test_c
        OLD_ID                          0
        OLD_F01                         0
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del:test_a
        Records affected: 5
        TRG_NAME                        TRG_TEST
        REL_NAME                        V_TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert or update or delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        T_SOURCE                        b
        ID                              1
        F01                             1
        T_SOURCE                        c
        ID                              2
        F01                             4
        Records affected: 2
        MSG                             test_10. Trigger with exception
        Statement failed, SQLSTATE = HY000
        exception 1
        -EXC_TEST
        -Something wrong occurs: -10
        OLD_ID                          -10
        OLD_F01                         999
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_11. Trigger with post event
        OLD_ID                          -10
        OLD_F01                         999
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_12. Trigger that changes a table for which it was created (recursion)
        Statement failed, SQLSTATE = 54001
        Too many concurrent executions of the same request
        At tr...
        CURR_GEN                        1001
        Records affected: 0
        Records affected: 0
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_13. Attempt to create `AFTER` trigger must fail if `new.` is changed
        Statement failed, SQLSTATE = 42000
        attempted update of read-only column
        ID                              -13
        F01                             -13
        Records affected: 0
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP TRIGGER TRG_TEST failed
        -Trigger TRG_TEST not found
        MSG                             test_14. Attempt to create `ON INSERT` trigger must fail if `old.` presents in its source
        Statement failed, SQLSTATE = 42S22
        unsuccessful metadata update
        -CREATE TRIGGER TRG_TEST failed
        -Dynamic SQL Error
        -SQL error code = -206
        -Column unknown
        -OLD.ID
        -At line 5, column 68
        ID                              -14
        F01                             -14
        Records affected: 1
        Records affected: 0
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP TRIGGER TRG_TEST failed
        -Trigger TRG_TEST not found
        MSG                             test_15. Attempt to create `ON DELETE` trigger must fail if `new.` presents in its source
        Statement failed, SQLSTATE = 42S22
        unsuccessful metadata update
        -CREATE TRIGGER TRG_TEST failed
        -Dynamic SQL Error
        -SQL error code = -206
        -Column unknown
        -NEW.ID
        -At line 5, column 68
        Records affected: 0
        Records affected: 0
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP TRIGGER TRG_TEST failed
        -Trigger TRG_TEST not found
    """

    expected_stdout_4x = f"""
        MSG                             test_01. Trigger BEFORE INSERT
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          1
        NEW_F01                         1
        DML_INFO                        ins
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_02. Trigger AFTER INSERT
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          2
        NEW_F01                         200
        DML_INFO                        ins
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_03. Trigger BEFORE UPDATE
        OLD_ID                          -3
        OLD_F01                         299
        NEW_ID                          3
        NEW_F01                         300
        DML_INFO                        upd
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before update
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_04. Trigger AFTER UPDATE
        OLD_ID                          -4
        OLD_F01                         399
        NEW_ID                          4
        NEW_F01                         400
        DML_INFO                        upd
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after update
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_05. Trigger BEFORE DELETE
        OLD_ID                          -5
        OLD_F01                         499
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_06. Trigger AFTER DELETE
        OLD_ID                          -6
        OLD_F01                         599
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_07. Trigger in INACTIVE state
        Records affected: 0
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         INACTIVE
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_08. Trigger with specifying its POSITION. Check result for several triggers
        OLD_ID                          -8
        OLD_F01                         799
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del_pos_1
        OLD_ID                          -8
        OLD_F01                         799
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del_pos_2
        Records affected: 2
        TRG_NAME                        TRG_TEST_A
        REL_NAME                        TEST
        TRG_SEQN                        2
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_NAME                        TRG_TEST_B
        REL_NAME                        TEST
        TRG_SEQN                        1
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_09. Trigger for a VIEW
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          0
        NEW_F01                         0
        DML_INFO                        ins:test_a
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          1
        NEW_F01                         1
        DML_INFO                        ins:test_b
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          2
        NEW_F01                         2
        DML_INFO                        ins:test_c
        OLD_ID                          2
        OLD_F01                         2
        NEW_ID                          2
        NEW_F01                         4
        DML_INFO                        upd:test_c
        OLD_ID                          0
        OLD_F01                         0
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del:test_a
        Records affected: 5
        TRG_NAME                        TRG_TEST
        REL_NAME                        V_TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert or update or delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        T_SOURCE                        b
        ID                              1
        F01                             1
        T_SOURCE                        c
        ID                              2
        F01                             4
        Records affected: 2
        MSG                             test_10. Trigger with exception
        Statement failed, SQLSTATE = HY000
        exception 1
        -EXC_TEST
        -Something wrong occurs: -10
        OLD_ID                          -10
        OLD_F01                         999
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_11. Trigger with post event
        OLD_ID                          -10
        OLD_F01                         999
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_12. Trigger that changes a table for which it was created (recursion)
        Statement failed, SQLSTATE = 54001
        Too many concurrent executions of the same request
        At tr...
        CURR_GEN                        1000
        Records affected: 0
        Records affected: 0
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_13. Attempt to create `AFTER` trigger must fail if `new.` is changed
        Statement failed, SQLSTATE = 42000
        attempted update of read-only column TEST.ID
        ID                              -13
        F01                             -13
        Records affected: 0
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP TRIGGER TRG_TEST failed
        -Trigger TRG_TEST not found
        MSG                             test_14. Attempt to create `ON INSERT` trigger must fail if `old.` presents in its source
        Statement failed, SQLSTATE = 42S22
        unsuccessful metadata update
        -CREATE TRIGGER TRG_TEST failed
        -Dynamic SQL Error
        -SQL error code = -206
        -Column unknown
        -OLD.ID
        -At line 5, column 68
        ID                              -14
        F01                             -14
        Records affected: 1
        Records affected: 0
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP TRIGGER TRG_TEST failed
        -Trigger TRG_TEST not found
        MSG                             test_15. Attempt to create `ON DELETE` trigger must fail if `new.` presents in its source
        Statement failed, SQLSTATE = 42S22
        unsuccessful metadata update
        -CREATE TRIGGER TRG_TEST failed
        -Dynamic SQL Error
        -SQL error code = -206
        -Column unknown
        -NEW.ID
        -At line 5, column 68
        Records affected: 0
        Records affected: 0
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP TRIGGER TRG_TEST failed
        -Trigger TRG_TEST not found
        MSG                             test_16. Trigger that calls routine based on external engine (UDR)
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          2023
        NEW_F01                         0
        DML_INFO                        ins
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          2024
        NEW_F01                         1
        DML_INFO                        ins
        Records affected: 2
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_17. Trigger with SQL SECURITY clause
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          1
        NEW_F01                         1
        DML_INFO                        ins
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
    """

    expected_stdout_6x = """
        MSG                             test_01. Trigger BEFORE INSERT
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          1
        NEW_F01                         1
        DML_INFO                        ins
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_02. Trigger AFTER INSERT
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          2
        NEW_F01                         200
        DML_INFO                        ins
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_03. Trigger BEFORE UPDATE
        OLD_ID                          -3
        OLD_F01                         299
        NEW_ID                          3
        NEW_F01                         300
        DML_INFO                        upd
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before update
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_04. Trigger AFTER UPDATE
        OLD_ID                          -4
        OLD_F01                         399
        NEW_ID                          4
        NEW_F01                         400
        DML_INFO                        upd
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after update
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_05. Trigger BEFORE DELETE
        OLD_ID                          -5
        OLD_F01                         499
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_06. Trigger AFTER DELETE
        OLD_ID                          -6
        OLD_F01                         599
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_07. Trigger in INACTIVE state
        Records affected: 0
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         INACTIVE
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_08. Trigger with specifying its POSITION. Check result for several triggers
        OLD_ID                          -8
        OLD_F01                         799
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del_pos_1
        OLD_ID                          -8
        OLD_F01                         799
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del_pos_2
        Records affected: 2
        TRG_NAME                        TRG_TEST_A
        REL_NAME                        TEST
        TRG_SEQN                        2
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TRG_NAME                        TRG_TEST_B
        REL_NAME                        TEST
        TRG_SEQN                        1
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_09. Trigger for a VIEW
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          0
        NEW_F01                         0
        DML_INFO                        ins:test_a
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          1
        NEW_F01                         1
        DML_INFO                        ins:test_b
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          2
        NEW_F01                         2
        DML_INFO                        ins:test_c
        OLD_ID                          2
        OLD_F01                         2
        NEW_ID                          2
        NEW_F01                         4
        DML_INFO                        upd:test_c
        OLD_ID                          0
        OLD_F01                         0
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del:test_a
        Records affected: 5
        TRG_NAME                        TRG_TEST
        REL_NAME                        V_TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert or update or delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        T_SOURCE                        b
        ID                              1
        F01                             1
        T_SOURCE                        c
        ID                              2
        F01                             4
        Records affected: 2
        MSG                             test_10. Trigger with exception
        Statement failed, SQLSTATE = HY000
        exception 1
        -"PUBLIC"."EXC_TEST"
        -Something wrong occurs: -10
        OLD_ID                          -10
        OLD_F01                         999
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_11. Trigger with post event
        OLD_ID                          -10
        OLD_F01                         999
        NEW_ID                          <null>
        NEW_F01                         <null>
        DML_INFO                        del
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_12. Trigger that changes a table for which it was created (recursion)
        Statement failed, SQLSTATE = 54001
        Too many concurrent executions of the same request
        At trigger ...
        CURR_GEN                        1000
        Records affected: 0
        Records affected: 0
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_13. Attempt to create `AFTER` trigger must fail if `new.` is changed
        Statement failed, SQLSTATE = 42000
        attempted update of read-only column "PUBLIC"."TEST"."ID"
        ID                              -13
        F01                             -13
        Records affected: 0
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP TRIGGER "PUBLIC"."TRG_TEST" failed
        -Trigger "PUBLIC"."TRG_TEST" not found
        MSG                             test_14. Attempt to create `ON INSERT` trigger must fail if `old.` presents in its source
        Statement failed, SQLSTATE = 42S22
        unsuccessful metadata update
        -CREATE TRIGGER "PUBLIC"."TRG_TEST" failed
        -Dynamic SQL Error
        -SQL error code = -206
        -Column unknown
        -"OLD"."ID"
        -At line 6, column 68
        ID                              -14
        F01                             -14
        Records affected: 1
        Records affected: 0
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP TRIGGER "PUBLIC"."TRG_TEST" failed
        -Trigger "PUBLIC"."TRG_TEST" not found
        MSG                             test_15. Attempt to create `ON DELETE` trigger must fail if `new.` presents in its source
        Statement failed, SQLSTATE = 42S22
        unsuccessful metadata update
        -CREATE TRIGGER "PUBLIC"."TRG_TEST" failed
        -Dynamic SQL Error
        -SQL error code = -206
        -Column unknown
        -"NEW"."ID"
        -At line 6, column 68
        Records affected: 0
        Records affected: 0
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -DROP TRIGGER "PUBLIC"."TRG_TEST" failed
        -Trigger "PUBLIC"."TRG_TEST" not found
        MSG                             test_16. Trigger that calls routine based on external engine (UDR)
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          2023
        NEW_F01                         0
        DML_INFO                        ins
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          2024
        NEW_F01                         1
        DML_INFO                        ins
        Records affected: 2
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
        MSG                             test_17. Trigger with SQL SECURITY clause
        OLD_ID                          <null>
        OLD_F01                         <null>
        NEW_ID                          1
        NEW_F01                         1
        DML_INFO                        ins
        Records affected: 1
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        TRG_SCHEMA                      PUBLIC
        TS_BEFORE_DML_START             0001-01-01 00:00:00.0000
        TS_AT_END_OF_TRIGGER            0001-01-01 00:00:00.0000
        INTERVAL_MS                     OK
    """

    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_4x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches = ['-q'], input = test_script, combine_output= True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
