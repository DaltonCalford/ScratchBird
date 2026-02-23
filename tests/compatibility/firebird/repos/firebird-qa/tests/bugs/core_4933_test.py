#coding:utf-8

"""
ID:          issue-5224
ISSUE:       5224
TITLE:       Add better transaction control to isql
DESCRIPTION:
  Test creates two .sql script and run them using ISQL utility.
  In the 1st script we create view for check current transaction parameters.
  View output following values for transaction:
    TIL, lock resolution (wait/no_wait/lock_timeout), read_only/read_write and [no]auto_undo

  Then we TURN ON keeping of Tx parameters (SET KEEP_TRAN ON) and do some manipulations in this
  ('main') script, including invocation of auxiliary ('addi') script using IN <...> command.

  Second script creates another database and the same view in it, then does some actions there
  and also check output of this view.
  After this (second) script finish, we return to 1st one and resume there final actions.

  IN ALL STEPS WE HAVE TO SEE THE SAME PARAMS - NO MATTER HOW MUCH TIMES
  WE DID COMMIT/ROLLBACK/RECONNECT AND EVEN WORK IN OTHER DB.
JIRA:        CORE-4933
FBTEST:      bugs.core_4933
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    MSG                             main_script: initial
    TX_TIL_MON_TRANS                snapshot
    TX_TIL_RDB_GET_CONTEXT          SNAPSHOT
    TX_LOCK_TIMEOUT_MON_TRANS       wait
    TX_LOCK_TIMEOUT_RDB_GET_CONTEXT -1
    TX_READ_ONLY_MON_TRANS          read_write
    TX_READ_ONLY_RDB_GET_CONTEXT    FALSE
    TX_AUTOUNDO_MON_TRANS           1


    MSG                             main_script: started Tx
    TX_TIL_MON_TRANS                rc rec_vers
    TX_TIL_RDB_GET_CONTEXT          READ COMMITTED
    TX_LOCK_TIMEOUT_MON_TRANS       timeout 5
    TX_LOCK_TIMEOUT_RDB_GET_CONTEXT 5
    TX_READ_ONLY_MON_TRANS          read_only
    TX_READ_ONLY_RDB_GET_CONTEXT    TRUE
    TX_AUTOUNDO_MON_TRANS           0


    MSG                             main_script: after_commit
    TX_TIL_MON_TRANS                rc rec_vers
    TX_TIL_RDB_GET_CONTEXT          READ COMMITTED
    TX_LOCK_TIMEOUT_MON_TRANS       timeout 5
    TX_LOCK_TIMEOUT_RDB_GET_CONTEXT 5
    TX_READ_ONLY_MON_TRANS          read_only
    TX_READ_ONLY_RDB_GET_CONTEXT    TRUE
    TX_AUTOUNDO_MON_TRANS           0


    MSG                             main_script: after_rollback
    TX_TIL_MON_TRANS                rc rec_vers
    TX_TIL_RDB_GET_CONTEXT          READ COMMITTED
    TX_LOCK_TIMEOUT_MON_TRANS       timeout 5
    TX_LOCK_TIMEOUT_RDB_GET_CONTEXT 5
    TX_READ_ONLY_MON_TRANS          read_only
    TX_READ_ONLY_RDB_GET_CONTEXT    TRUE
    TX_AUTOUNDO_MON_TRANS           0


    MSG                             main_script: after_reconnect
    TX_TIL_MON_TRANS                rc rec_vers
    TX_TIL_RDB_GET_CONTEXT          READ COMMITTED
    TX_LOCK_TIMEOUT_MON_TRANS       timeout 5
    TX_LOCK_TIMEOUT_RDB_GET_CONTEXT 5
    TX_READ_ONLY_MON_TRANS          read_only
    TX_READ_ONLY_RDB_GET_CONTEXT    TRUE
    TX_AUTOUNDO_MON_TRANS           0


    MSG                             addi_script: create_new_db
    TX_TIL_MON_TRANS                rc rec_vers
    TX_TIL_RDB_GET_CONTEXT          READ COMMITTED
    TX_LOCK_TIMEOUT_MON_TRANS       timeout 5
    TX_LOCK_TIMEOUT_RDB_GET_CONTEXT 5
    TX_READ_ONLY_MON_TRANS          read_only
    TX_READ_ONLY_RDB_GET_CONTEXT    TRUE
    TX_AUTOUNDO_MON_TRANS           0


    MSG                             addi_script: reconnect
    TX_TIL_MON_TRANS                rc rec_vers
    TX_TIL_RDB_GET_CONTEXT          READ COMMITTED
    TX_LOCK_TIMEOUT_MON_TRANS       timeout 5
    TX_LOCK_TIMEOUT_RDB_GET_CONTEXT 5
    TX_READ_ONLY_MON_TRANS          read_only
    TX_READ_ONLY_RDB_GET_CONTEXT    TRUE
    TX_AUTOUNDO_MON_TRANS           0


    MSG                             main_script: resume
    TX_TIL_MON_TRANS                rc rec_vers
    TX_TIL_RDB_GET_CONTEXT          READ COMMITTED
    TX_LOCK_TIMEOUT_MON_TRANS       timeout 5
    TX_LOCK_TIMEOUT_RDB_GET_CONTEXT 5
    TX_READ_ONLY_MON_TRANS          read_only
    TX_READ_ONLY_RDB_GET_CONTEXT    TRUE
    TX_AUTOUNDO_MON_TRANS           0


    MSG                             keep_tran: turned_off
    TX_TIL_MON_TRANS                snapshot
    TX_TIL_RDB_GET_CONTEXT          SNAPSHOT
    TX_LOCK_TIMEOUT_MON_TRANS       wait
    TX_LOCK_TIMEOUT_RDB_GET_CONTEXT -1
    TX_READ_ONLY_MON_TRANS          read_write
    TX_READ_ONLY_RDB_GET_CONTEXT    FALSE
    TX_AUTOUNDO_MON_TRANS           1
"""


addi_script = temp_file('addi_script.sql')
main_script = temp_file('main_script.sql')
tmp_db = temp_file('tmp_addi_4933.fdb')

@pytest.mark.version('>=3.0.6')
def test_1(act: Action, addi_script: Path, main_script: Path, tmp_db: Path):
    addi_script.write_text(
        f"""
            create database 'localhost:{tmp_db}' user {act.db.user} password '{act.db.password}';

            recreate view v_check as
            select
                 decode(t.mon$isolation_mode, 0,'consistency', 1,'snapshot', 2,'rc rec_vers', 3,'rc no_recv', 4,'rc read_cons', 'UNKNOWN') as tx_til_mon_trans,
                 rdb$get_context('SYSTEM', 'ISOLATION_LEVEL') as tx_til_rdb_get_context,
                 decode(t.mon$lock_timeout, -1, 'wait', 0, 'no_wait', 'timeout ' || t.mon$lock_timeout) as tx_lock_timeout_mon_trans,
                 rdb$get_context('SYSTEM', 'LOCK_TIMEOUT') as tx_lock_timeout_rdb_get_context,
                 iif(t.mon$read_only=1,'read_only','read_write') as tx_read_only_mon_trans,
                 rdb$get_context('SYSTEM', 'READ_ONLY') as tx_read_only_rdb_get_context,
                 t.mon$auto_undo as tx_autoundo_mon_trans
                -- only in FB 4.x+: ,t.mon$auto_commit as tx_autocommit_mon_trans
            from mon$transactions t
            where t.mon$transaction_id = current_transaction;
            commit;

            select 'addi_script: create_new_db' as msg, v.* from v_check v;
            rollback;

            connect 'localhost:{tmp_db}' user {act.db.user} password '{act.db.password}';
            select 'addi_script: reconnect' as msg, v.* from v_check v;
            rollback;

            drop database;
        """
    )
    main_script.write_text(
        f"""
            set list on;
            connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}';
            recreate view v_check as
            select
                 decode(t.mon$isolation_mode, 0,'consistency', 1,'snapshot', 2,'rc rec_vers', 3,'rc no_recv', 4,'rc read_cons', 'UNKNOWN') as tx_til_mon_trans,
                 rdb$get_context('SYSTEM', 'ISOLATION_LEVEL') as tx_til_rdb_get_context,
                 decode(t.mon$lock_timeout, -1, 'wait', 0, 'no_wait', 'timeout ' || t.mon$lock_timeout) as tx_lock_timeout_mon_trans,
                 rdb$get_context('SYSTEM', 'LOCK_TIMEOUT') as tx_lock_timeout_rdb_get_context,
                 iif(t.mon$read_only=1,'read_only','read_write') as tx_read_only_mon_trans,
                 rdb$get_context('SYSTEM', 'READ_ONLY') as tx_read_only_rdb_get_context,
                 t.mon$auto_undo as tx_autoundo_mon_trans
                -- only 4.x: ,t.mon$auto_commit as tx_autocommit_mon_trans
            from mon$transactions t
            where t.mon$transaction_id = current_transaction;
            commit;

            select 'main_script: initial' as msg, v.* from v_check v;
            commit;

            set keep_tran on;
            commit;

            set transaction read only read committed record_version lock timeout 5 no auto undo; -- only in 4.x: auto commit;

            select 'main_script: started Tx' as msg, v.* from v_check v;

            commit; -------------------------------------------------------------------------------------- [ 1 ]

            select 'main_script: after_commit' as msg, v.* from v_check v;

            rollback; ------------------------------------------------------------------------------------ [ 2 ]

            select 'main_script: after_rollback' as msg, v.* from v_check v;

            rollback;

            connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}'; --------------------------- [ 3 ]

            select 'main_script: after_reconnect' as msg, v.* from v_check v;
            rollback;

            --###################
            in {addi_script};
            --###################

            connect '{act.db.dsn}' user {act.db.user} password '{act.db.password}'; --------------------------- [ 5 ]

            select 'main_script: resume' as msg, v.* from v_check v;
            rollback;

            set keep_tran off;
            commit;

            select 'keep_tran: turned_off' as msg, v.* from v_check v;
            commit;
        """
    )
    # Check
    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input_file = main_script, connect_db = False, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
