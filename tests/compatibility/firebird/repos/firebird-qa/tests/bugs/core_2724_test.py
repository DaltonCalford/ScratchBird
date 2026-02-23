#coding:utf-8

"""
ID:          issue-3120
ISSUE:       3120
TITLE:       Validate or transform string of DML queries so that engine internals doesn't receive malformed strings
DESCRIPTION:
  Code from doc/sql.extensions/README.ddl_triggers.txt was taken as basis for this test
  (see ticket issue: "This situation happened with DDL triggers ...").
  Several DB objects are created here and their DDL contain unicode (Greek) text.
  Attachment these issues these DDL intentionally is run with charset = NONE.
  This charset (NONE) should result in question marks after we finish DDL and want to query log table
  that was filled by DDL trigger and contains issued DDL statements.
JIRA:        CORE-2724
FBTEST:      bugs.core_2724
"""

import pytest
from firebird.qa import *

init_script = """
    create sequence ddl_seq;

    create table ddl_log (
        id bigint not null primary key,
        moment timestamp not null,
        current_connection_cset varchar(31) not null,
        event_type varchar(25) not null,
        object_type varchar(25) not null,
        ddl_event varchar(25) not null,
        object_name varchar(31) not null,
        old_object_name varchar(31),
        new_object_name varchar(31),
        sql_text blob sub_type text not null,
        ok char(1) not null,
        result_info blob sub_type text
    );
    commit;

    set term ^;
    create trigger trig_ddl_log_before before any ddl statement
    as
        declare id type of column ddl_log.id;
        declare v_current_connection_cset varchar(31);
    begin
        -- We do the changes in an AUTONOMOUS TRANSACTION, so if an exception happens and the command
        -- didn't run, the log will survive.
        in autonomous transaction do
        begin

            select coalesce(c.rdb$character_set_name, '??? NULL ???')
            from mon$attachments a
            left join rdb$character_sets c on a.mon$character_set_id = c.rdb$character_set_id
            where a.mon$attachment_id = current_connection
            into v_current_connection_cset;

            insert into ddl_log (id, moment, current_connection_cset,
                                 event_type, object_type, ddl_event, object_name,
                                 old_object_name, new_object_name, sql_text, ok, result_info)
                values (next value for ddl_seq,
                        'now',
                        :v_current_connection_cset,
                        rdb$get_context('DDL_TRIGGER', 'EVENT_TYPE'),
                        rdb$get_context('DDL_TRIGGER', 'OBJECT_TYPE'),
                        rdb$get_context('DDL_TRIGGER', 'DDL_EVENT'),
                        rdb$get_context('DDL_TRIGGER', 'OBJECT_NAME'),
                        rdb$get_context('DDL_TRIGGER', 'OLD_OBJECT_NAME'),
                        rdb$get_context('DDL_TRIGGER', 'NEW_OBJECT_NAME'),
                        rdb$get_context('DDL_TRIGGER', 'SQL_TEXT'),
                        'N',
                        'Κάτι συνέβη. Θα πρέπει να ελέγξετε') -- Something was wrong. One need to check this.
                returning id into id;
            rdb$set_context('USER_SESSION', 'trig_ddl_log_id', id);
        end
    end
    ^

    -- Note: the above trigger will fire for this DDL command. It's good idea to use -nodbtriggers
    -- when working with them!
    create trigger trig_ddl_log_after after any ddl statement
    as
    begin
        -- Here we need an AUTONOMOUS TRANSACTION because the original transaction will not see the
        -- record inserted on the BEFORE trigger autonomous transaction if user transaction is not
        -- READ COMMITTED.
        in autonomous transaction do
            update ddl_log set ok = 'Y',
            result_info = 'Τα πάντα ήταν επιτυχής' -- Everything has completed successfully
            where id = rdb$get_context('USER_SESSION', 'trig_ddl_log_id');
    end
    ^
    set term ;^
    commit;

    -- So lets delete the record about trig_ddl_log_after creation.
    delete from ddl_log;
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

act = python_act('db', substitutions=[('SQL_TEXT .*', 'SQL_TEXT'),
                                      ('RESULT_INFO .*', 'RESULT_INFO')])

expected_stdout_a = """
    ID                              2
    CURRENT_CONNECTION_CSET         NONE
    SQL_TEXT
    create domain dm_name varchar(50) check (value in ('??????????????????', '??????????', '????????????', '??????????????', '????????????????'))
    RESULT_INFO
    Τα πάντα ήταν επιτυχής
    DDL_EVENT                       CREATE DOMAIN
    OBJECT_NAME                     DM_NAME

    ID                              3
    CURRENT_CONNECTION_CSET         NONE
    SQL_TEXT
    recreate table t1 (
             saller_id integer  -- ?????????????????????????? ?????????????? // ID of saler
            ,customer_id integer  -- ?????????????????????????? ???????????? // ID of customer
            ,product_name dm_name
        )
    RESULT_INFO
    Τα πάντα ήταν επιτυχής
    DDL_EVENT                       CREATE TABLE
    OBJECT_NAME                     T1
"""

expected_stdout_b = """
    ID                              6
    CURRENT_CONNECTION_CSET         UTF8
    SQL_TEXT                        80:0
    create domain dm_name varchar(50) check (value in ('αμορτισέρ', 'κόμβο', 'σωλήνα', 'φέροντα', 'βραχίονα'))
    RESULT_INFO                     80:2
    Τα πάντα ήταν επιτυχής
    DDL_EVENT                       CREATE DOMAIN
    OBJECT_NAME                     DM_NAME

    ID                              7
    CURRENT_CONNECTION_CSET         UTF8
    SQL_TEXT
    recreate table t1 (
             saller_id integer  -- αναγνωριστικό εμπόρου // ID of saler
            ,customer_id integer  -- αναγνωριστικό πελάτη // ID of customer
            ,product_name dm_name
        )
    RESULT_INFO
    Τα πάντα ήταν επιτυχής
    DDL_EVENT                       CREATE TABLE
    OBJECT_NAME                     T1
"""

sql_check = """
delete from ddl_log;
commit;

create domain dm_name varchar(50) check (value in ('αμορτισέρ', 'κόμβο', 'σωλήνα', 'φέροντα', 'βραχίονα'));
recreate table t1 (
     saller_id integer  -- αναγνωριστικό εμπόρου // ID of saler
    ,customer_id integer  -- αναγνωριστικό πελάτη // ID of customer
    ,product_name dm_name
);
commit;
set list on;

select id, current_connection_cset, sql_text, result_info, ddl_event, object_name
from ddl_log order by id;

commit;
drop table t1;
drop domain dm_name;
exit;
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    #
    act.expected_stdout = expected_stdout_a
    act.isql(switches=[], charset='NONE', input=sql_check)
    assert act.clean_stdout == act.clean_expected_stdout
    #
    act.reset()
    act.expected_stdout = expected_stdout_b
    act.isql(switches=[], charset='UTF8', input=sql_check)
    assert act.clean_stdout == act.clean_expected_stdout
