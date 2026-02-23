#coding:utf-8

"""
ID:          session.alter-session-reset-start-new-tx
ISSUE:       6093
TITLE:       ALTER SESSION RESET: START new transaction with the same properties as transaction
  that was rolled back (if transaction was present before reset)
DESCRIPTION:
  Test issue about ALTER SESSION RESET:
  "START new transaction with the same properties as transaction that was rolled back (if
  transaction was present before reset)".

  We create trivial table and insert one row in it with ID = current_transaction - and use
  AUTONOMOUS tranaction for this (ES is used).
  Then, without committing changes in main Tx, we issue 'ALTER SESSION RESET'.
  Warning must be thrown after it (in STDERR) and no records must remain in the table as result.
  After this, we check that new transaction has ID different than used before and that attributes of this Tx were NOT changed.

  NOTE. *** SET AUTODDL OFF REQUIRED ***
  Following is detailed explanation of this note:

    Default ISQL behaviour is to start always *TWO* transactions (for DML and second for DDL)
    after previous commit / rollback and before *ANY* further satement is to be executed, except
    those which control ISQL itself (e.g. 'SET TERM'; 'IN ...'; 'SET BAIL' etc).
    So, even when statement <S> has nothing to change, ISQL will start TWO transactions
    just before executing <S>.
    This means that these transactions will start even if want to run 'ALTER SESSION RESET'.
    This, in turn, makes one of them (which must perform DDL) be 'active and NOT current'
    from ALTER SESSION point of view (which is run within DML transaction).

    According to description given in #6093, ALTER SESSION throws error isc_ses_reset_err
    "if any open transaction exist in current conneciton, *except of current transaction* and
    prepared 2PC transactions which is allowed and ignored by this check".

    So, we have to prohibit 'autostart' of DDL-transaction because otherwise ALTER SESSION will
    throw: "SQLSTATE = 01002 / Cannot reset user session / -There are open transactions (2 active)".
    This is done by 'SET AUTODDL OFF' statement at the beginning of this test script.

  Thanks to Vlad for explanations (discussed 18.01.2021).
FBTEST:      functional.session.alter_session_reset_start_new_tx
JIRA:        CORE-5832
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- set echo on;
    set bail on;
    recreate table test(
        trn_id bigint
        ,trn_isolation_mode smallint
        ,trn_lock_timeout int
        ,trn_read_only smallint
        ,trn_auto_undo smallint
    );
    commit;

    set autoddl off;
    commit;

    set transaction read committed record_version lock timeout 5 no auto undo;
    set term ^;
    execute block as
        declare v_isolation_mode type of column mon$transactions.mon$isolation_mode;
        declare v_lock_timeout type of column mon$transactions.mon$lock_timeout;
        declare v_read_only type of column mon$transactions.mon$read_only;
        declare v_auto_undo type of column mon$transactions.mon$auto_undo;
    begin
        select
            mon$isolation_mode
           ,mon$lock_timeout
           ,mon$read_only
           ,mon$auto_undo
        from mon$transactions
        where mon$transaction_id = current_transaction
        into
            v_isolation_mode
           ,v_lock_timeout
           ,v_read_only
           ,v_auto_undo
        ;
        in autonomous transaction do
        insert into test(trn_id,              trn_isolation_mode, trn_lock_timeout, trn_read_only, trn_auto_undo)
                  values(current_transaction, :v_isolation_mode,  :v_lock_timeout,  :v_read_only,  :v_auto_undo );
    end
    ^
    set term ;^

    --------------------
    alter session reset;
    --------------------

    set term ^;
    execute block as
        declare v_isolation_mode type of column mon$transactions.mon$isolation_mode;
        declare v_lock_timeout type of column mon$transactions.mon$lock_timeout;
        declare v_read_only type of column mon$transactions.mon$read_only;
        declare v_auto_undo type of column mon$transactions.mon$auto_undo;
    begin
        select
            mon$isolation_mode
           ,mon$lock_timeout
           ,mon$read_only
           ,mon$auto_undo
        from mon$transactions
        where mon$transaction_id = current_transaction
        into
            v_isolation_mode
           ,v_lock_timeout
           ,v_read_only
           ,v_auto_undo
        ;
        insert into test(trn_id,              trn_isolation_mode, trn_lock_timeout, trn_read_only, trn_auto_undo)
                  values(current_transaction, :v_isolation_mode,  :v_lock_timeout,  :v_read_only,  :v_auto_undo );
    end
    ^
    set term ;^

    set list on;
    select
        count(distinct trn_id) as trn_id_distinct_cnt
        ,count(distinct trn_isolation_mode) as trn_isolation_mode_distinct_cnt
        ,count(distinct trn_lock_timeout) as trn_lock_timeout_distinct_cnt
        ,count(distinct trn_read_only) as trn_read_distinct_cnt
        ,count(distinct trn_auto_undo) as trn_auto_undo_distinct_cnt
    from test;

"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    TRN_ID_DISTINCT_CNT             2
    TRN_ISOLATION_MODE_DISTINCT_CNT 1
    TRN_LOCK_TIMEOUT_DISTINCT_CNT   1
    TRN_READ_DISTINCT_CNT           1
    TRN_AUTO_UNDO_DISTINCT_CNT      1
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
