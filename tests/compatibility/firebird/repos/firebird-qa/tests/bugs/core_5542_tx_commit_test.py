#coding:utf-8

"""
ID:          issue-5810-C
ISSUE:       5810
TITLE:       Database-level triggers related to TRANSACTION events (i.e. start, commit and
  rollback) do not take in account their POSITION index (when more than one trigger for
  the same event type is defined)
DESCRIPTION:
  This test does check only for 'TRANSACTION COMMIT' case.
JIRA:        CORE-5542
FBTEST:      bugs.core_5542_tx_commit
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    set term ^;
    create or alter trigger trg_tx_commit_beta inactive on transaction commit position 0 as begin end
    ^
    create or alter trigger trg_tx_commit_anna inactive on transaction commit position 0 as begin end
    ^
    create or alter trigger trg_tx_commit_ciao inactive on transaction commit position 0 as begin end
    ^
    commit
    ^
    drop trigger trg_tx_commit_beta
    ^
    drop trigger trg_tx_commit_anna
    ^
    drop trigger trg_tx_commit_ciao
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
    create or alter trigger trg_tx_commit_ciao active on transaction commit position 111 as
        declare c int;
    begin
        if ( rdb$get_context('USER_SESSION','RUN_DB_LEVEL_TRG') is not null ) then
        begin
            --select count(*) from rdb$types,rdb$types,(select 1 i from rdb$types rows 3) into c;
            rdb$set_context('USER_SESSION','RUN_DB_LEVEL_TRG_CIAO', gen_id(g,1) || ': TRG_TX_COMMIT_CIAO ');
        end
    end
    ^

    create or alter trigger trg_tx_commit_anna active on transaction commit position 3 as
        declare c int;
    begin
        if ( rdb$get_context('USER_SESSION','RUN_DB_LEVEL_TRG') is not null ) then
        begin
            --select count(*) from rdb$types,rdb$types,(select 1 i from rdb$types rows 3) into c;
            rdb$set_context('USER_SESSION','RUN_DB_LEVEL_TRG_ANNA', gen_id(g,1) || ': TRG_TX_COMMIT_ANNA ');
        end
    end
    ^

    create or alter trigger trg_tx_commit_beta active on transaction commit position 22 as
        declare c int;
    begin
        if ( rdb$get_context('USER_SESSION','RUN_DB_LEVEL_TRG') is not null ) then
        begin
            --select count(*) from rdb$types,rdb$types,(select 1 i from rdb$types rows 3) into c;
            rdb$set_context('USER_SESSION','RUN_DB_LEVEL_TRG_BETA', gen_id(g,1) || ': TRG_TX_COMMIT_BETA ');
        end
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
    where r.rdb$trigger_name starting with upper('trg_tx_commit')
    order by r.rdb$trigger_sequence
    ;


    set term ^;
    execute block as
    begin
      rdb$set_context('USER_SESSION','RUN_DB_LEVEL_TRG', 1); -- Allow triggers to assign values to corresp. context variables 'RUN_DB_LEVEL_TRG_*'
    end
    ^
    set term ;^


    set count on;
    commit;

    select
        c.MON$VARIABLE_NAME
       ,c.MON$VARIABLE_VALUE
    from mon$context_variables c
    where c.mon$attachment_id = current_connection and c.mon$variable_name similar to '%(ANNA|BETA|CIAO)'
    order by c.mon$variable_VALUE
    ;

"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$TRIGGER_NAME                TRG_TX_COMMIT_ANNA
    RDB$TRIGGER_SEQUENCE            3
    RDB$TRIGGER_TYPE                8195
    RDB$TRIGGER_NAME                TRG_TX_COMMIT_BETA
    RDB$TRIGGER_SEQUENCE            22
    RDB$TRIGGER_TYPE                8195
    RDB$TRIGGER_NAME                TRG_TX_COMMIT_CIAO
    RDB$TRIGGER_SEQUENCE            111
    RDB$TRIGGER_TYPE                8195
    MON$VARIABLE_NAME               RUN_DB_LEVEL_TRG_ANNA
    MON$VARIABLE_VALUE              1: TRG_TX_COMMIT_ANNA
    MON$VARIABLE_NAME               RUN_DB_LEVEL_TRG_BETA
    MON$VARIABLE_VALUE              2: TRG_TX_COMMIT_BETA
    MON$VARIABLE_NAME               RUN_DB_LEVEL_TRG_CIAO
    MON$VARIABLE_VALUE              3: TRG_TX_COMMIT_CIAO
    Records affected: 3
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

