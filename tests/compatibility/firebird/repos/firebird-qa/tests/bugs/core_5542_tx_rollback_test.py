#coding:utf-8

"""
ID:          issue-5810-D
ISSUE:       5810
TITLE:       Database-level triggers related to TRANSACTION events (i.e. start, commit and
  rollback) do not take in account their POSITION index (when more than one trigger for
  the same event type is defined)
DESCRIPTION:
  This test does check only for 'TRANSACTION ROLLBACK' case.
JIRA:        CORE-5542
FBTEST:      bugs.core_5542_tx_rollback
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    set term ^;
    create or alter trigger trg_tx_cancel_beta inactive on transaction rollback position 0 as begin end
    ^
    create or alter trigger trg_tx_cancel_anna inactive on transaction rollback position 0 as begin end
    ^
    create or alter trigger trg_tx_cancel_ciao inactive on transaction rollback position 0 as begin end
    ^
    commit
    ^
    drop trigger trg_tx_cancel_beta
    ^
    drop trigger trg_tx_cancel_anna
    ^
    drop trigger trg_tx_cancel_ciao
    ^

    recreate table trg_log( id integer, msg varchar(20) );
    ^

    execute block as
    begin
        execute statement 'drop sequence g';
        when any do begin end
    end
    ^
    set term ;^
    commit;

    connect '$(DSN)' user 'SYSDBA' password 'masterkey';

    set autoddl off;
    set bail on;

    create sequence g;
    commit;

    set term ^;
    create trigger trg_log_bi for trg_log active before insert position 0 as
    begin
        new.id = coalesce( new.id, gen_id(g,1) );
    end
    ^
    set term ;^
    commit;


    set term ^;
    create or alter trigger trg_tx_cancel_ciao active on transaction rollback position 111 as
    begin
        if ( rdb$get_context('USER_SESSION','RUN_DB_LEVEL_TRG') is not null ) then
            in autonomous transaction do
            insert into trg_log(msg) values('trigger tx_ciao');
    end
    ^

    create or alter trigger trg_tx_cancel_anna active on transaction rollback position 3 as
    begin
        if ( rdb$get_context('USER_SESSION','RUN_DB_LEVEL_TRG') is not null ) then
            in autonomous transaction do
            insert into trg_log(msg) values('trigger tx_anna');
    end
    ^

    create or alter trigger trg_tx_cancel_beta active on transaction rollback position 22 as
    begin
        if ( rdb$get_context('USER_SESSION','RUN_DB_LEVEL_TRG') is not null ) then
            in autonomous transaction do
            insert into trg_log(msg) values('trigger tx_beta');
    end
    ^

    set term ;^
    commit;

    select
         r.rdb$trigger_name
        ,r.rdb$trigger_sequence
        ,r.rdb$trigger_type
    from rdb$triggers r
    where r.rdb$trigger_name starting with upper('trg_tx_cancel')
    order by r.rdb$trigger_sequence
    ;


    set term ^;
    execute block as
    begin
      rdb$set_context('USER_SESSION','RUN_DB_LEVEL_TRG', 1);
    end
    ^
    set term ;^

    set count on;
    rollback;
    select * from trg_log;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$TRIGGER_NAME                TRG_TX_CANCEL_ANNA
    RDB$TRIGGER_SEQUENCE            3
    RDB$TRIGGER_TYPE                8196
    RDB$TRIGGER_NAME                TRG_TX_CANCEL_BETA
    RDB$TRIGGER_SEQUENCE            22
    RDB$TRIGGER_TYPE                8196
    RDB$TRIGGER_NAME                TRG_TX_CANCEL_CIAO
    RDB$TRIGGER_SEQUENCE            111
    RDB$TRIGGER_TYPE                8196
    ID                              1
    MSG                             trigger tx_anna
    ID                              2
    MSG                             trigger tx_beta
    ID                              3
    MSG                             trigger tx_ciao
    Records affected: 3
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

