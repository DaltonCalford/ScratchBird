#coding:utf-8

"""
ID:          trigger.database.transaction-start-multiple-with-normal-finish
TITLE:       Multiple triggers on start transaction
DESCRIPTION:
  This tests normal operation of database TRANSACTION START trigger when:
      1) more than one such triggers are defined
      2) NO exception raise within any trigger during its work, i.e. all of them shoudl finish Ok

      Triggers must work within the same Tx as "parent" statement (which launched this Tx).
FBTEST:      functional.trigger.database.transaction_start_multiple_with_normal_finish
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;

    recreate table trg_log(id integer, trg_tx int default current_transaction, rows_inserted_before int);
    commit;

    set autoddl off;

    set term ^;

    execute block as
      declare n int;
      declare i int = 1;
      declare ddl varchar(255);
    begin

      rdb$set_context('USER_SESSION','TRIGGERS_TO_CREATE', 5); -- <<< ---------------- HOW MANY TRIGGERS ARE DEFINED

      n = cast( rdb$get_context('USER_SESSION','TRIGGERS_TO_CREATE') as int);
      while ( i <= n ) do
      begin
        ddl = 'create or alter trigger trg_start_tx_'|| i ||' on transaction start position 0 as '
              || 'begin '
              || '  insert into trg_log(id, rows_inserted_before) values('|| i ||',  (select count(*) from trg_log) ); '
              || 'end'
        ;
        execute statement (ddl);
        i = i + 1;
      end
    end
    ^
    set term ;^

    --select current_timestamp from rdb$database;
    commit;
    --select current_timestamp from rdb$database;

    -- Following 'select' sttm leads to implicit Tx start ==> all db-level triggers 'on transaction start' will be fired
    -- before engine begin to execute this statement.
    -- Table trg_log should contain rdb$get_context('USER_SESSION','TRIGGERS_TO_CREATE') records and they all had to be
    -- created within current Tx:

    select iif( count(distinct t.id) = max(t.expected_count),
                'EXPECTED.',
                'WRONG: ' || count(distinct id) || ' instead of ' || max(t.expected_count)
              ) as triggers_fired_count
    from
    (
        select t.id, t.trg_tx, cast( rdb$get_context('USER_SESSION','TRIGGERS_TO_CREATE') as int) as expected_count
        from trg_log t
    ) t
    where trg_tx = current_transaction
    ;

    select id,rows_inserted_before from trg_log order by id;

    rollback;

    -- Previous rollback should remove from trg_log all rows that were inserted there by triggers:
    select count(distinct id) as rows_after_rollback
    from trg_log where trg_tx is distinct from current_transaction;

    commit;

    -- Previous commit should save in trg_log two records that were inserted there by triggers:
    select iif( count(distinct t.id) = max(expected_count),
                'EXPECTED.',
                'WRONG: ' || count(distinct id) || ' instead of ' || max(t.expected_count)
              ) as rows_after_commit
    from
    (
        select t.id, t.trg_tx, cast( rdb$get_context('USER_SESSION','TRIGGERS_TO_CREATE') as int) as expected_count
        from trg_log t
    ) t
    where trg_tx is distinct from current_transaction;
"""

act = isql_act('db', test_script)

expected_stdout = """
    TRIGGERS_FIRED_COUNT            EXPECTED.

    ID                              1
    ROWS_INSERTED_BEFORE            0
    ID                              2
    ROWS_INSERTED_BEFORE            1
    ID                              3
    ROWS_INSERTED_BEFORE            2
    ID                              4
    ROWS_INSERTED_BEFORE            3
    ID                              5
    ROWS_INSERTED_BEFORE            4

    ROWS_AFTER_ROLLBACK             0
    ROWS_AFTER_COMMIT               EXPECTED.
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
