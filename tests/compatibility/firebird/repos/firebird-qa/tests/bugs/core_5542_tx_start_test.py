#coding:utf-8

"""
ID:          issue-5810-E
ISSUE:       5810
TITLE:       Database-level triggers related to TRANSACTION events (i.e. start, commit and
  rollback) do not take in account their POSITION index (when more than one trigger for
  the same event type is defined)
DESCRIPTION:
  This test does check only for 'TRANSACTION START' case.
JIRA:        CORE-5542
FBTEST:      bugs.core_5542_tx_start
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    set term ^;
    create or alter trigger trg_tx_start_beta inactive on transaction start position 0 as begin end
    ^
    create or alter trigger trg_tx_start_anna inactive on transaction start position 0 as begin end
    ^
    create or alter trigger trg_tx_start_ciao inactive on transaction start position 0 as begin end
    ^
    commit
    ^
    drop trigger trg_tx_start_beta
    ^
    drop trigger trg_tx_start_anna
    ^
    drop trigger trg_tx_start_ciao
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
    create or alter trigger trg_tx_start_ciao active on transaction start position 111 as
    begin
        if ( rdb$get_context('USER_SESSION','RUN_DB_LEVEL_TRG') is not null ) then
            insert into trg_log(msg) values('trigger tx_ciao');
    end
    ^

    create or alter trigger trg_tx_start_anna active on transaction start position 3 as
    begin
        if ( rdb$get_context('USER_SESSION','RUN_DB_LEVEL_TRG') is not null ) then
            insert into trg_log(msg) values('trigger tx_anna');
    end
    ^

    create or alter trigger trg_tx_start_beta active on transaction start position 22 as
    begin
        if ( rdb$get_context('USER_SESSION','RUN_DB_LEVEL_TRG') is not null ) then
            insert into trg_log(msg) values('trigger tx_beta');
    end
    ^

    set term ;^
    commit;

    -- Check (preview):
    select
         r.rdb$trigger_name
        ,r.rdb$trigger_sequence
        ,r.rdb$trigger_type
    from rdb$triggers r
    where r.rdb$trigger_name starting with upper('TRG_TX_START')
    order by r.rdb$trigger_sequence
    ;


    set term ^;
    execute block as
    begin
      rdb$set_context('USER_SESSION','RUN_DB_LEVEL_TRG', 1); -- this allows triggers to do INSERTs in the TRG_LOG table.
    end
    ^
    set term ;^

    commit;

    -- Following 'select' statement causes IMPLICIT start of transaction. It means that all triggers with names "trg_tx_start_nnnn"
    -- will fire. They all should run within current Tx, so we could see results of their activity in the table 'trg_log':
    set count on;
    select * from trg_log;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$TRIGGER_NAME                TRG_TX_START_ANNA
    RDB$TRIGGER_SEQUENCE            3
    RDB$TRIGGER_TYPE                8194
    RDB$TRIGGER_NAME                TRG_TX_START_BETA
    RDB$TRIGGER_SEQUENCE            22
    RDB$TRIGGER_TYPE                8194
    RDB$TRIGGER_NAME                TRG_TX_START_CIAO
    RDB$TRIGGER_SEQUENCE            111
    RDB$TRIGGER_TYPE                8194
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

