#coding:utf-8

"""
ID:          n/a
TITLE:       ALTER TRIGGER - basic checks for DML triggers
DESCRIPTION:
    Test checks several scnarios with 'ALTER TRIGGER' statement, they are titled in 'msg_map' dict.
    Statements can cause either successful outcome or raise exception because of some rule(s) violation.
    We check content of RDB$ tables in order to see data for triggers(s) INSTEAD of usage 'SHOW DOMAIN' command.
    View 'v_trig_info' is used to show all data related to domains.
    Its DDL differs for FB versions prior/ since 6.x (columns related to SQL schemas present for 6.x).
NOTES:
    [12.07.2025] pzotov
    This test replaces previously created ones with names:
        test_01.py  test_06.py  test_11.py
        test_02.py  test_07.py  test_12.py
        test_03.py  test_08.py  test_13.py
        test_04.py  test_09.py
        test_05.py  test_10.py 
    All these tests has been marked to be SKIPPED from execution.
    ::: NB :::
    Several questions raised during implementing this test. Sent Q to dimitr et al,
    letters 12.07.2025 19:40, 20:29, subject: "Exception not raises when trigger time / mutation_list is changed..."
    Checked on 6.0.0.909; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()
substitutions = [('attempted update of read-only column .*', 'attempted update of read-only column'), ('[ \t]+', ' ')]
act = isql_act('db', substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    TRG_SCHEMA_COLUMN = '' if act.is_version('<6') else ',rdb$schema_name as trg_schema'
    TRG_SCHEMA_FLDVAL = '' if act.is_version('<6') else 'TRG_SCHEMA                      PUBLIC'
    READ_ONLY_COLUMN = 'TEST.ID' if act.is_version('<6') else '"PUBLIC"."TEST"."ID"'

    msg_map = {
        'test_01' : 'Change trigger status from inactive to active'
       ,'test_02' : 'Change trigger status from active to inactive'
       ,'test_03' : "Change trigger time/event from `after update` to `before delete`"
       ,'test_04' : "Change trigger time/event from `before update` to `after delete`"
       ,'test_07' : "Attempt to change time of trigger `before insert/update` to `after delete` must fail if `new.` is changed in old code"
       ,'test_08' : "Alter trigger position: ability to specify new value within SMALLINT scope"
       ,'test_09' : "Alter trigger position: check result for two triggers"
       ,'test_10' : "Alter trigger should be allowed without specifying any other attributes"
       ,'test_12' : "Attempt to change ON DELETE/UPDATE trigger to ON INSERT without changing its source must fail if `old.` remains"
       ,'test_13' : "Attempt to change ON INSERT/UPDATE trigger to ON DELETE without changing its source must fail if `new.` remains"
    }
    for k,v in msg_map.items():
        msg_map[k] = '. '.join( (k,v) )

    test_script = f"""
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

        -- set echo on;

        create table test(id integer not null constraint unq unique, text varchar(80));
        commit;
        ----------------------------
        select '{msg_map["test_01"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test inactive before insert as
        begin
          new.id=1;
        end ^
        set term ;^
        commit;
        alter trigger trg_test active;
        commit;
        select * from v_trig_info;
        delete from test;
        commit;
        ----------------------------
        select '{msg_map["test_02"]}' as msg from rdb$database;
        alter trigger trg_test inactive;
        commit;
        select * from v_trig_info;
        delete from test;
        commit;
        drop trigger trg_test;
        ----------------------------
        select '{msg_map["test_03"]}' as msg from rdb$database;
        create trigger trg_test for test after update as begin end;
        alter trigger trg_test before delete;
        commit;
        select * from v_trig_info;
        delete from test;
        commit;
        drop trigger trg_test;
        ----------------------------
        select '{msg_map["test_04"]}' as msg from rdb$database;
        create trigger trg_test for test before update as begin end;
        alter trigger trg_test after delete;
        commit;
        select * from v_trig_info;
        delete from test;
        commit;
        drop trigger trg_test;
        ----------------------------
        select '{msg_map["test_07"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test active before insert as
        begin
          new.id=1;
        end ^
        set term ;^
        commit;
        alter trigger trg_test after delete; -- must FAIL
        commit;
        select * from v_trig_info;

        -- WEIRD! RECONNECT REQUIRED HERE OTHERWISE IT CONTINUES FAIL WITH 
        -- Statement failed, SQLSTATE = 42000 / attempted update of read-only column
        -- Sent letter to dimitr et al, 12.07.2025 20:29
        insert into test(id) values(1);
        delete from test;
        commit;
        drop trigger trg_test;
        ----------------------------
        select '{msg_map["test_08"]}' as msg from rdb$database;
        create trigger trg_test for test before update as begin end;
        alter trigger trg_test position 32767;
        commit;
        select * from v_trig_info;
        delete from test;
        commit;
        drop trigger trg_test;
        ----------------------------
        select '{msg_map["test_09"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test_a for test before insert position 1 as
        begin
            new.text = new.text || ' trg_test_a';
        end
        ^
        create trigger trg_test_b for test before insert position 10 as
        begin
            new.text = new.text || ' trg_test_b';
        end
        ^
        set term ;^
        commit;
        insert into test(id, text) values(1, 'point-1:');
        commit;
        alter trigger trg_test_b position 0;
        commit;
        insert into test(id, text) values(2, 'point-1:');
        commit;
        select * from test order by id;
        select * from v_trig_info;
        delete from test;
        commit;
        drop trigger trg_test_a;
        drop trigger trg_test_b;
        ----------------------------
        select '{msg_map["test_10"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test active before insert as
        begin
              new.text = 'initial';
        end ^
        set term ;^
        commit;
        insert into test(id) values(1);
        commit;

        set term ^;
        alter trigger trg_test as
        begin
            new.text = 'altered';
        end ^
        set term ;^
        commit;
        insert into test(id) values(2);
        commit;
        select * from test order by id;
        select * from v_trig_info;
        delete from test;
        commit;
        drop trigger trg_test;
        ----------------------------
        -- https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref50/firebird-50-language-reference.html#fblangref50-psql-oldnew
        -- "In INSERT triggers, references to OLD are invalid and will throw an exception"
        -- Attempt to change ON DELETE/UPDATE trigger to ON INSERT without changing its source must fail if `old.` remains
        -- 12.07.2025: IT'S STRANGE BUT THIS RULE SEEMS NOT WORK. Sent letter to dimitr et al, 12.07.2025 19:40
        select '{msg_map["test_12"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test active after delete as
            declare v int;
        begin
            v = old.id;
        end
        ^
        alter trigger trg_test before insert -- must FAIL ?
        ^
        set term ;^
        commit;
        select * from v_trig_info;
        delete from test;
        commit;
        drop trigger trg_test;
        ----------------------------
        -- https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref50/firebird-50-language-reference.html#fblangref50-psql-oldnew
        -- In DELETE triggers, references to NEW are invalid and will throw an exception
        -- Attempt to change ON INSERT/UPDATE trigger to ON DELETE without changing its source must fail if `new.` remains
        -- 12.07.2025: IT'S STRANGE BUT THIS RULE SEEMS NOT WORK. Sent letter to dimitr et al, 12.07.2025 19:40
        select '{msg_map["test_13"]}' as msg from rdb$database;
        set term ^;
        create trigger trg_test for test before update as
            declare v int;
        begin
            v = new.id;
        end
        ^
        alter trigger trg_test after delete -- must FAIL ?
        ^
        set term ;^
        commit;
        select * from v_trig_info;
        delete from test;
        commit;
        drop trigger trg_test;
    """

    expected_stdout = f"""
        MSG                             {msg_map['test_01']}
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        {TRG_SCHEMA_FLDVAL}

        MSG                             {msg_map['test_02']}
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         INACTIVE
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        {TRG_SCHEMA_FLDVAL}

        MSG                             {msg_map['test_03']}
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        {TRG_SCHEMA_FLDVAL}

        MSG                             {msg_map['test_04']}
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        {TRG_SCHEMA_FLDVAL}

        MSG                             {msg_map['test_07']}
        Statement failed, SQLSTATE = 42000
        attempted update of read-only column {READ_ONLY_COLUMN}
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        {TRG_SCHEMA_FLDVAL}

        Statement failed, SQLSTATE = 42000
        attempted update of read-only column {READ_ONLY_COLUMN}

        MSG                             {msg_map['test_08']}
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        32767
        TRG_ACT                         active
        TRG_TYPE                        before update
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        {TRG_SCHEMA_FLDVAL}

        MSG                             {msg_map['test_09']}
        ID                              1
        TEXT                            point-1: trg_test_a trg_test_b
        ID                              2
        TEXT                            point-1: trg_test_b trg_test_a
        TRG_NAME                        TRG_TEST_A
        REL_NAME                        TEST
        TRG_SEQN                        1
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        {TRG_SCHEMA_FLDVAL}

        TRG_NAME                        TRG_TEST_B
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        {TRG_SCHEMA_FLDVAL}

        MSG                             {msg_map['test_10']}
        ID                              1
        TEXT                            initial
        ID                              2
        TEXT                            altered

        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        {TRG_SCHEMA_FLDVAL}

        MSG                             {msg_map['test_12']}
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        before insert
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        {TRG_SCHEMA_FLDVAL}

        MSG                             {msg_map['test_13']}
        TRG_NAME                        TRG_TEST
        REL_NAME                        TEST
        TRG_SEQN                        0
        TRG_ACT                         active
        TRG_TYPE                        after delete
        TRG_VALID_BLR                   1
        TRG_ENGINE                      <null>
        TRG_ENTRY                       <null>
        {TRG_SCHEMA_FLDVAL}
    """
    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_script, combine_output= True)
    assert act.clean_stdout == act.clean_expected_stdout
