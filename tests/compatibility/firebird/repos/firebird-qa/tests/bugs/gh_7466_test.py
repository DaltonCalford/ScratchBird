#coding:utf-8

"""
ID:          issue-7466
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7466
TITLE:       Add COMPILE trace events for procedures/functions/triggers
DESCRIPTION:
    Test prepares trace config with requrement to see events related to units compilation.
    We create standalone procedure, standalone function and package with procedure and function.
    Also, we create three type of triggers: for a table, for DB-level event ('on connect') and for any DDL statement.
    Then test launches trace session and runs ISQL with appropriate script with above mentioned actions.

    Finally, we parse trace log and filter only lines containing names of created PSQL units which we know.
    No errors must present in the trace log. All created units must be specified in blocks related to compilation.
NOTES:
    [17-aug-2023] pzotov
        ::: NB :::
        0. This test DOES NOT check tracking of plans for queries inside those PSQL modules (i.e. strarting ticket issue,
           see: https://github.com/FirebirdSQL/firebird/pull/7466#issue-1564439735 ).
           SEPARATE TEST WILL BE IMPLEMENTED FOR THAT.
        1. The term 'COMPILE' means parsing of BLR code into an execution tree, i.e. this action
           occurs when unit code is loaded into metadata cache. 
        2. Procedures and functions are loaded into metadata cache immediatelly when they are created.
        3. Triggers are loaded into metadata cache in 'deferred' way, when something occurs that causes trigger to fire.
           So, DML trigger will fire when we do (for example) INSERT, DB_level trigger - when we do some action on DB level
           (e.g. connect/disconnect), and similar to DDL trigger.
        4. Currently there is no way to specify in the trace what EXACT type of DDL trigger fired. It is shown as "AFTER DDL".
        5. Lot of system-related triggers are displayed in the trace log during creating user-defined units:
               Trigger RDB$TRIGGER_26 FOR RDB$RELATION_CONSTRAINTS
               Trigger RDB$TRIGGER_18 FOR RDB$INDEX_SEGMENTS (BEFORE UPDATE)
               Trigger RDB$TRIGGER_8 FOR RDB$USER_PRIVILEGES (BEFORE DELETE)
           etc. Test ignores them and takes in account only triggers that have been creates by "our" SQL script.
        6. User-defined DDL trigger will be loaded into metadata cache MULTIPLE times (three in this test: for create view,
           its altering and its dropping - although there is no re-connect between these actions). This is conisdered as bug,
           see: https://github.com/FirebirdSQL/firebird/pull/7426 (currently it is not yet fixed).
        
        Checked on 5.0.0.1164.
        Thanks to dimitr for explanations.
        Discussed with dimitr, letters 17.08.2023.

    [06-sep-2023] pzotov
        Changed expected output: DDL trigger is loaded into metadata cache only once, so we have to check only SINGLE
        occurence of "Trigger TRG_DDL (AFTER DDL)" event.
        See also: https://github.com/FirebirdSQL/firebird/commit/00c2d10102468d5494b413c0de295079f62a27ec
        Checked on 5.0.0.1190
    [05.07.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Checked on 6.0.0.909; 5.0.3.1668.
"""
import locale
import re
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

trace = ['log_initfini = false',
         'log_errors = true',
         'log_procedure_compile = true',
         'log_function_compile = true',
         'log_trigger_compile = true',
         ]

allowed_patterns = [
     ' ERROR AT ',
     'Trigger ("PUBLIC".)?(")?TRG_',
     'Procedure ("PUBLIC".)?(")?(SP_TEST|PG_TEST(")?.(")?PG_SP_WORKER(")?)',
     'Function ("PUBLIC".)?(")?(FN_TEST|PG_TEST(")?.(")?PG_FN_WORKER)'
]
allowed_patterns = [ re.compile(r, re.IGNORECASE) for r in  allowed_patterns]

@pytest.mark.trace
@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):

    test_script = f"""
        set autoddl off;

        recreate sequence g;

        create table att_log (
          msg varchar(60)
          ,dts timestamp default 'now'
        );

        recreate table ddl_log (
            id integer,
            ddl_event varchar(25),
            sql blob sub_type text
        );

        recreate table test(id int primary key, x int, y int);
        create index test_x on test(x);
        create index test_y on test(y);
        commit;

        create or alter view v_init as
            select count(*) as cnt from test group by x
            rows 1
        ;

        create or alter view v_worker as
            select count(*) as cnt
            from test
            group by y
            plan (TEST ORDER TEST_Y)
            union all
            select cnt from v_init
        ;
        commit;

        set term ^;

        --##################################################################

        create or alter procedure sp_test (
            a_table varchar(63)
            ,a_field varchar(63)
        ) returns (
            o_field_len int
        ) as
            declare procedure sp_test_inner(a_x int) returns(o_y int) as
            begin
                for select y from test where x = :a_x into o_y do suspend;
            end
        begin
            for
                select f.rdb$field_length
                from rdb$relation_fields rf
                join rdb$fields f on rf.rdb$field_source=f.rdb$field_name
                where rf.rdb$relation_name = upper(:a_table) and rf.rdb$field_name=upper(:a_field)
            into o_field_len
            do
                suspend;
        end
        ^

        --##################################################################

        create or alter function fn_test (
            a_table varchar(63)
            ,a_field varchar(63)
        ) returns int
        as
            declare function fn_test_inner(a_x int) returns int as
            begin
                return ( select count(*) from test where x = :a_x );
            end
        begin
            return (
                select first 1 f.rdb$field_length
                from rdb$relation_fields rf
                join rdb$fields f on rf.rdb$field_source=f.rdb$field_name
                where rf.rdb$relation_name = upper(:a_table) and rf.rdb$field_name=upper(:a_field)
            );
        end
        ^

        --##################################################################

        create or alter package pg_test as
        begin
            function pg_fn_worker returns int;
            procedure pg_sp_worker;
        end
        ^
        recreate package body pg_test as
        begin
            function pg_fn_worker returns int as
                declare function fn_test_inner_pg(a_x int) returns int as
                begin
                    return ( select count(*) from test where x = :a_x );
                end
            begin
                return (
                    select sum(cnt)
                    from (
                        select count(*) as cnt
                        from test group by x
                        plan (TEST ORDER TEST_X)
                        union all
                        select cnt from v_worker
                    )
                );
            end

            procedure pg_sp_worker as
                declare c int;
                declare procedure sp_test_inner_pg(a_x int) returns(o_y int) as
                begin
                    for select y from test where x = :a_x into o_y do suspend;
                end
            begin
                select sum(cnt)
                from (
                    select count(*) as cnt
                    from test group by x
                    plan (TEST ORDER TEST_X)
                    union all
                    select cnt from v_worker
                )
                into c
                ;
            end

        end
        ^

        --##################################################################

        -- DML trigger:
        create trigger trg_test_biu for test before insert or update as
        begin
            if (inserting) then
                new.id = coalesce(new.id, gen_id(g,1));
            new.y = minvalue(new.y, new.x * new.x);
        end
        ^

        --##################################################################

        -- DB level trigger:
        create trigger trg_db_conn on connect
        as
        begin
          if (current_user = 'SYSDBA') then
          begin
            in autonomous transaction
            do
            begin
              insert into att_log (msg) values ( current_user || ' connected');
            end
          end
        end
        ^

        --##################################################################

        -- DDL trigger:
        create or alter trigger trg_ddl after any ddl statement
        as
        begin
          insert into ddl_log(sql, ddl_event)
            values (rdb$get_context('DDL_TRIGGER', 'SQL_TEXT'),
                    rdb$get_context('DDL_TRIGGER', 'DDL_EVENT') );
        end
        ^

        set term ;^
        commit;

        set autoddl on;

        connect '{act.db.dsn}';

        insert into test(x, y) select rand()*100, rand()*100 from rdb$types rows 10;
        commit;

        create view v_test as select * from test;
        alter view v_test as select * from rdb$database;
        drop view v_test;
        commit;
    """

    with act.trace(db_events=trace, encoding = locale.getpreferredencoding(), encoding_errors='utf8'):
        act.isql(switches = ['-q'], input = test_script, combine_output = True, io_enc = locale.getpreferredencoding())

    # Process trace
    for line in act.trace_log:
        if line.rstrip().split():
            for p in allowed_patterns:
                if p.search(line):
                    print(line.strip())

    expected_stdout_5x = f"""
        Procedure SP_TEST:
        Procedure PG_TEST.PG_SP_WORKER:
        Function FN_TEST:
        Function PG_TEST.PG_FN_WORKER:
        Trigger TRG_DB_CONN (ON CONNECT):
        Trigger TRG_TEST_BIU FOR TEST (BEFORE INSERT):
        Trigger TRG_DDL (AFTER DDL):
    """

    expected_stdout_6x = f"""
        Procedure "PUBLIC"."SP_TEST":
        Procedure "PUBLIC"."PG_TEST"."PG_SP_WORKER":
        Function "PUBLIC"."FN_TEST":
        Function "PUBLIC"."PG_TEST"."PG_FN_WORKER":
        Trigger "PUBLIC"."TRG_DB_CONN" (ON CONNECT):
        Trigger "PUBLIC"."TRG_TEST_BIU" FOR "PUBLIC"."TEST" (BEFORE INSERT):
        Trigger "PUBLIC"."TRG_DDL" (AFTER DDL):
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
