#coding:utf-8

"""
ID:          trigger.database.transaction-start
TITLE:       Trigger on start tansaction
DESCRIPTION:
  This tests normal operation of database TRANSACTION START trigger.
FBTEST:      functional.trigger.database.transactionstart_01
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set autoddl off;
    create table trg_log(id integer, trg_tx int default current_transaction);

    create view v_check as
    select count(*) as cnt_chk_tx, count(iif(trg_tx=current_transaction,1,null)) as cnt_chk_trg
    from trg_log
    where trg_tx = current_transaction;

    set term ^;
    create trigger trg_start_tx inactive on transaction start position 0 as
    begin
        insert into trg_log default values;
        --insert into trg_log(trg_tx) values (current_transaction);
    end
    ^
    set term ;^
    commit;

    alter trigger trg_start_tx active;
    commit;

    set list on;
    set autoddl off;

    select 'Tx to be rolled back' as phase
           --, current_transaction
    from rdb$database;
    --select a.* from trg_log a;
    select * from v_check;
    rollback;


    select 'Tx to be committed' as phase
           --, current_transaction
    from rdb$database;
    --select a.* from trg_log a;
    select * from v_check;
    commit;

    select 'Final select' as phase
           --, current_transaction
    from rdb$database;
    --select a.* from trg_log a order by id desc rows 1;
    select * from v_check;

    set term ^;
    alter trigger trg_start_tx inactive position 0 as
    begin
        insert into trg_log(trg_tx) values (1/0);
    end
    ^
    set term ;^
    commit;

    alter trigger trg_start_tx active;
    commit;

    commit; -- this should raise exception in trg_start_tx and this exception SHOULD PASS to the client.
"""

act = isql_act('db', test_script, substitutions=[('line: \\d+, col: \\d+', '')])

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    TEST_TRG_NAME = "'TRG_START_TX'" if act.is_version('<6') else f'{SQL_SCHEMA_PREFIX}"TRG_START_TX"'
    expected_stdout = f"""
        PHASE                           Tx to be rolled back
        CNT_CHK_TX                      1
        CNT_CHK_TRG                     1
        PHASE                           Tx to be committed
        CNT_CHK_TX                      1
        CNT_CHK_TRG                     1
        PHASE                           Final select
        CNT_CHK_TX                      1
        CNT_CHK_TRG                     1
        Statement failed, SQLSTATE = 22012
        arithmetic exception, numeric overflow, or string truncation
        -Integer divide by zero.  The code attempted to divide an integer value by an integer divisor of zero.
        -At trigger {TEST_TRG_NAME}
    """

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
