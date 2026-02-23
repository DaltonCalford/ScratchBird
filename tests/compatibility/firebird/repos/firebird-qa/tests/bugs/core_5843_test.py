#coding:utf-8

"""
ID:          issue-6104
ISSUE:       6104
TITLE:       Wrong handling of failures of TRANSACTION START trigger
DESCRIPTION:
JIRA:        CORE-5843
FBTEST:      bugs.core_5843
NOTES:
    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    --set echo on;
    set bail on;
    create or alter trigger tx_rollback on transaction rollback as begin end;
    create or alter trigger tx_start on transaction start as begin end;
    create or alter trigger tx_commit on transaction commit as begin end;

    create or alter view v_mon as
    select
        t.mon$transaction_id as tx_id
        ,t.mon$state as tx_state
        --,a.mon$remote_process att_process
        --,a.mon$attachment_name as att_name
        --,a.mon$system_flag as att_sysflag
        --,a.mon$user as att_user
    from mon$transactions t
    join mon$attachments a using (mon$attachment_id)
    where mon$attachment_id = current_connection
    ;
    commit;

    set term ^;

    create or alter exception ex_tra_start 'transaction @1 aborted'
    ^

    create or alter procedure tx_trig_log(msg varchar(255))
    as
    declare s varchar(255);
    begin
        s = rdb$get_context('USER_SESSION', 'tx_trig_log');
        if (s <> '') then
        begin
            s = s || ASCII_CHAR(10) || trim(msg) || ', current tx=' || current_transaction;
            rdb$set_context('USER_SESSION', 'tx_trig_log', :s);
        end
    end
    ^

    create or alter trigger tx_start on transaction start as
    declare s varchar(255);
    begin
        execute procedure tx_trig_log('trigger on transaction start ');

        if (rdb$get_context('USER_SESSION', 'tx_abort') = 1) then
        begin
            execute procedure tx_trig_log('exception on tx start ');
            rdb$set_context('USER_SESSION', 'tx_abort', 0);
            exception ex_tra_start using (current_transaction);
        end
    end
    ^

    create or alter trigger tx_commit on transaction commit as
        declare s varchar(255);
    begin
        execute procedure tx_trig_log('trigger on commit');
    end
    ^

    create or alter trigger tx_rollback on transaction rollback as
        declare s varchar(255);
    begin
        execute procedure tx_trig_log('trigger on rollback');
    end
    ^

    create or alter procedure sp_use_atx as
    begin
        rdb$set_context('USER_SESSION', 'tx_abort', 1);
        execute procedure tx_trig_log('sp_use_atx, point_a');
        in autonomous transaction do
            execute procedure tx_trig_log('sp_use_atx, point_b');
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set autoddl off;
    set list on;
    commit;

    select 'test_1, point-1' as msg, v.* from rdb$database cross join v_mon v;

    set term ^;
    execute block as
        declare c int;
    begin
        c = rdb$set_context('USER_SESSION', 'tx_abort', 1); -- flag to raise error on next tx start
    end
    ^
    set term ;^
    commit;

    select 'test_1, point-2' as msg, v.* from rdb$database cross join v_mon v; -- it start transaction
    commit;

    set count on;
    select 'test_1, point-3' as msg, v.* from rdb$database cross join v_mon v;
    set count off;
    rollback;

    -------------------------------------------------------------

    --connect '$(DSN)' user 'SYSDBA' password 'masterkey';

    set term ^;
    execute block as
        declare c int;
    begin
        c = rdb$set_context('USER_SESSION', 'tx_trig_log', ascii_char(10) || 'start: tx=' || current_transaction ); -- start logging
    end
    ^
    set term ;^
    commit;

    select 'test2, point-a' as msg, v.* from rdb$database cross join v_mon v;

    execute procedure sp_use_atx;

    /*
        Statement failed, SQLSTATE = HY000
        exception 3
        -EX_TRA_START
        -transaction 692 aborted
        -At trigger 'TX_START' line: 10, col: 5
        At procedure 'sp_use_atx' line: 4, col: 3
    */

    select rdb$get_context('USER_SESSION', 'tx_trig_log') from rdb$database;

"""

substitutions = [('[ \t]+', ' '),
                 ('line[:]{0,1}[\\s]+[\\d]+.*', 'line'),
                 ('transaction[\\s]+[\\d]+[\\s]+aborted', 'transaction aborted'),
                 ('tx=[\\d]+', 'tx='), ('TX_ID[\\s]+[\\d]+', 'TX_ID'),
                 ('exception[\\s]+[\\d]+.*', 'exception')]

act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout_5x = """
    MSG                             test_1, point-1
    TX_ID                           19
    TX_STATE                        1
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_TRA_START
    -transaction 20 aborted
    -At trigger 'TX_START' line: 10, col: 13
    MSG                             test_1, point-3
    TX_ID                           22
    TX_STATE                        1
    Records affected: 1
    MSG                             test2, point-a
    TX_ID                           24
    TX_STATE                        1
    Statement failed, SQLSTATE = HY000
    exception 1
    -EX_TRA_START
    -transaction 25 aborted
    -At trigger 'TX_START' line: 10, col: 13
    At procedure 'SP_USE_ATX' line: 5, col: 9
    RDB$GET_CONTEXT
    start: tx=23
    trigger on commit, current tx=23
    trigger on transaction start, current tx=24
    sp_use_atx, point_a, current tx=24
    trigger on transaction start, current tx=25
    exception on tx start, current tx=25
"""

expected_stdout_6x = """
    MSG                             test_1, point-1
    TX_ID                           19
    TX_STATE                        1
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_TRA_START"
    -transaction 20 aborted
    -At trigger "PUBLIC"."TX_START" line: 10, col: 13
    MSG                             test_1, point-3
    TX_ID                           22
    TX_STATE                        1
    Records affected: 1
    MSG                             test2, point-a
    TX_ID                           24
    TX_STATE                        1
    Statement failed, SQLSTATE = HY000
    exception 1
    -"PUBLIC"."EX_TRA_START"
    -transaction 25 aborted
    -At trigger "PUBLIC"."TX_START" line: 10, col: 13
    At procedure "PUBLIC"."SP_USE_ATX" line: 5, col: 9
    RDB$GET_CONTEXT
    start: tx=23
    trigger on commit, current tx=23
    trigger on transaction start, current tx=24
    sp_use_atx, point_a, current tx=24
    trigger on transaction start, current tx=25
    exception on tx start, current tx=25
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
