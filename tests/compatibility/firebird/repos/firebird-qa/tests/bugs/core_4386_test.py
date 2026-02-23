#coding:utf-8

"""
ID:          issue-4708
ISSUE:       4708
TITLE:       Report more details for "object in use" errors
DESCRIPTION:
JIRA:        CORE-4386
FBTEST:      bugs.core_4386
NOTES:
    [22.11.2021] pcisar
      This test requires READ_COMMITTED_NO_RECORD_VERSION transaction to work, which
      requires ReadConsistency disabled in FB 4. However, it does not work as expected
      because all drop commands pass without exception even with ReadConsistency disabled.
      The same happens under 3.0.8 (no errors raised).
    [17.09.2022] pzotov
      1. Test actually must work identical for *every* isolation mode of all possible set.
      2. One need to be very careful with object that attempts to make COMMIT after DROP statement:
         if we use custom TPB, start transaction explicitly and 'bind' DDL cursor to this transaction
         then we have to run commit *exactly* by this TRANSACTION rather then connection whoch owns it!
         See 'tx2.commit()' in the code. If we replace it with 'con2.commit()' then Tx2 will be 
         *silently* rolled back (!!despite that we issued con.commit() !!) and we will not get any
         error messages. I'm not sure whether this correct or no.
      Checked on 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.730
    [22.08.2024] pzotov
      * Changed DDL because of SubQueryConversion config parameter appearance.
        We have to AVOID usage of queries which have plan that can be changed when firebird.conf has
        SubQueryConversion = true. In that case some index can be excluded from plan and thus
        it can be dropped on first iteration of 'for x_isol in tx_isol_lst' loop. This causes unexpected
        error 'index not found' for subsequent checks.
      * Added check for error message when we try to drop standalone function.
      * Assert moved out to the point after loop in order to show whole result in case of some error
        (rather than only one message block for some particular x_isol).
      * Excluded check of FB 3.x (this version no more changed).
      Checked on 6.0.0.442, 5.0.2.1479, 4.0.6.3142

    [29.06.2025] pzotov
      Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
      Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

db = db_factory()

act = python_act('db')

ddl_script = """
    set bail on;
    create or alter procedure sp_worker as begin end;
    create or alter procedure sp_test as begin end;
    create or alter view v_test as select 1 x from rdb$database;
    commit;

    recreate table test1(id int,x int);
    recreate table test2(id int,x int);
    commit;

    create index test1_id on test1(id);
    commit;
    create descending index test2_x on test2(x);
    commit;

    create or alter view v_test as select id,x from test1 where id between 15 and 30;
    commit;

    set term ^;
    create or alter function fn_worker(a_x int) returns int as
        declare v_id int;
    begin
        execute statement ('select max(b.id) from test2 b where b.x >= ?') (:a_x) into v_id;
        return v_id;
    end
    ^
    create or alter procedure sp_worker(a_id int) returns(x int) as
    begin
      for
          execute statement ('select v.x from v_test v where v.id = ? and v.id >= fn_worker(v.x)') (:a_id)
          into x
      do
          suspend;
    end
    ^
    create or alter procedure sp_test(a_id int) returns(x int) as
    begin
      for
          execute statement ('select x from sp_worker(?)') (:a_id)
          into x
      do
          suspend;
    end
    ^
    set term ;^
    commit;

    insert into test1 values(11,111);
    insert into test1 values(21,222);
    insert into test1 values(31,333);
    insert into test1 values(41,444);
    commit;

    insert into test2 select * from test1;
    commit;
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    act.isql(switches=[], input=ddl_script)

    drop_commands = [ 'drop procedure sp_test',
                      'drop procedure sp_worker',
                      'drop function fn_worker',
                      'drop view v_test',
                      'drop table test2',
                      'drop index test1_id',
                      'drop index test2_x'
                     ]

    tx_isol_lst = [ Isolation.READ_COMMITTED_NO_RECORD_VERSION,
                    Isolation.READ_COMMITTED_RECORD_VERSION,
                    Isolation.SNAPSHOT,
                    Isolation.SERIALIZABLE,
                  ]
    if act.is_version('>=4'):
       tx_isol_lst.append(Isolation.READ_COMMITTED_READ_CONSISTENCY)

    # for any isolation mode attempt to drop object that is in use by another Tx must fail
    # with the same error message. We check all possible Tx isolation modes for that:
    for x_isol in tx_isol_lst:

        with act.db.connect() as con1:

            cur1 = con1.cursor()
            cur1.execute('select x from sp_test(21)').fetchall()

            for cmd in drop_commands:
                with act.db.connect() as con2:
                    custom_tpb = tpb(isolation = x_isol, lock_timeout=0)
                    print(x_isol.name, cmd)
                    tx2 = con2.transaction_manager(custom_tpb)
                    tx2.begin()
                    cur2 = tx2.cursor()
                    try:
                        cur2.execute(cmd) # this will PASS because of DDL nature

                        ##########################################################################
                        ### We have to call commit() exactly by TRANSACTION object here.       ###
                        ### DO NOT use con2.commit() because this actually leads transaction   ###
                        ### to be 'silently rolled back', thus we will not get error messages! ###
                        ##########################################################################
                        tx2.commit() # <<< this lead to FAILED_COMMIT in the trace <<<

                    except Exception as e:
                        print(e.__str__())
                        print(e.gds_codes)

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    act.expected_stdout = f"""
        READ_COMMITTED_NO_RECORD_VERSION drop procedure sp_test
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object PROCEDURE {SQL_SCHEMA_PREFIX}"SP_TEST" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_NO_RECORD_VERSION drop procedure sp_worker
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object PROCEDURE {SQL_SCHEMA_PREFIX}"SP_WORKER" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_NO_RECORD_VERSION drop function fn_worker
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object FUNCTION {SQL_SCHEMA_PREFIX}"FN_WORKER" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_NO_RECORD_VERSION drop view v_test
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object VIEW {SQL_SCHEMA_PREFIX}"V_TEST" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_NO_RECORD_VERSION drop table test2
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object TABLE {SQL_SCHEMA_PREFIX}"TEST2" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_NO_RECORD_VERSION drop index test1_id
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object INDEX {SQL_SCHEMA_PREFIX}"TEST1_ID" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_NO_RECORD_VERSION drop index test2_x
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object INDEX {SQL_SCHEMA_PREFIX}"TEST2_X" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_RECORD_VERSION drop procedure sp_test
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object PROCEDURE {SQL_SCHEMA_PREFIX}"SP_TEST" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_RECORD_VERSION drop procedure sp_worker
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object PROCEDURE {SQL_SCHEMA_PREFIX}"SP_WORKER" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_RECORD_VERSION drop function fn_worker
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object FUNCTION {SQL_SCHEMA_PREFIX}"FN_WORKER" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_RECORD_VERSION drop view v_test
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object VIEW {SQL_SCHEMA_PREFIX}"V_TEST" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_RECORD_VERSION drop table test2
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object TABLE {SQL_SCHEMA_PREFIX}"TEST2" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_RECORD_VERSION drop index test1_id
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object INDEX {SQL_SCHEMA_PREFIX}"TEST1_ID" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_RECORD_VERSION drop index test2_x
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object INDEX {SQL_SCHEMA_PREFIX}"TEST2_X" is in use
        (335544345, 335544351, 335544453)

        SNAPSHOT drop procedure sp_test
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object PROCEDURE {SQL_SCHEMA_PREFIX}"SP_TEST" is in use
        (335544345, 335544351, 335544453)

        SNAPSHOT drop procedure sp_worker
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object PROCEDURE {SQL_SCHEMA_PREFIX}"SP_WORKER" is in use
        (335544345, 335544351, 335544453)

        SNAPSHOT drop function fn_worker
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object FUNCTION {SQL_SCHEMA_PREFIX}"FN_WORKER" is in use
        (335544345, 335544351, 335544453)

        SNAPSHOT drop view v_test
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object VIEW {SQL_SCHEMA_PREFIX}"V_TEST" is in use
        (335544345, 335544351, 335544453)

        SNAPSHOT drop table test2
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object TABLE {SQL_SCHEMA_PREFIX}"TEST2" is in use
        (335544345, 335544351, 335544453)

        SNAPSHOT drop index test1_id
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object INDEX {SQL_SCHEMA_PREFIX}"TEST1_ID" is in use
        (335544345, 335544351, 335544453)

        SNAPSHOT drop index test2_x
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object INDEX {SQL_SCHEMA_PREFIX}"TEST2_X" is in use
        (335544345, 335544351, 335544453)

        SERIALIZABLE drop procedure sp_test
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object PROCEDURE {SQL_SCHEMA_PREFIX}"SP_TEST" is in use
        (335544345, 335544351, 335544453)

        SERIALIZABLE drop procedure sp_worker
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object PROCEDURE {SQL_SCHEMA_PREFIX}"SP_WORKER" is in use
        (335544345, 335544351, 335544453)

        SERIALIZABLE drop function fn_worker
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object FUNCTION {SQL_SCHEMA_PREFIX}"FN_WORKER" is in use
        (335544345, 335544351, 335544453)

        SERIALIZABLE drop view v_test
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object VIEW {SQL_SCHEMA_PREFIX}"V_TEST" is in use
        (335544345, 335544351, 335544453)

        SERIALIZABLE drop table test2
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object TABLE {SQL_SCHEMA_PREFIX}"TEST2" is in use
        (335544345, 335544351, 335544453)

        SERIALIZABLE drop index test1_id
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object INDEX {SQL_SCHEMA_PREFIX}"TEST1_ID" is in use
        (335544345, 335544351, 335544453)

        SERIALIZABLE drop index test2_x
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object INDEX {SQL_SCHEMA_PREFIX}"TEST2_X" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_READ_CONSISTENCY drop procedure sp_test
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object PROCEDURE {SQL_SCHEMA_PREFIX}"SP_TEST" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_READ_CONSISTENCY drop procedure sp_worker
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object PROCEDURE {SQL_SCHEMA_PREFIX}"SP_WORKER" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_READ_CONSISTENCY drop function fn_worker
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object FUNCTION {SQL_SCHEMA_PREFIX}"FN_WORKER" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_READ_CONSISTENCY drop view v_test
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object VIEW {SQL_SCHEMA_PREFIX}"V_TEST" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_READ_CONSISTENCY drop table test2
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object TABLE {SQL_SCHEMA_PREFIX}"TEST2" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_READ_CONSISTENCY drop index test1_id
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object INDEX {SQL_SCHEMA_PREFIX}"TEST1_ID" is in use
        (335544345, 335544351, 335544453)

        READ_COMMITTED_READ_CONSISTENCY drop index test2_x
        lock conflict on no wait transaction
        -unsuccessful metadata update
        -object INDEX {SQL_SCHEMA_PREFIX}"TEST2_X" is in use
        (335544345, 335544351, 335544453)
    """
                
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
