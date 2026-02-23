#coding:utf-8

"""
ID:          issue-6268
ISSUE:       6268
TITLE:       Make it possible to start multiple transactions (possibly in different attachments)
  using the same initial transaction snapshot
DESCRIPTION:
  We open first connect using FDB and set custom transaction parameter block which is used to start SNAPSHOT transaction.
  Within this first transaction (tx1a) we insert into test table record with value = -2 and commit this Tx.
  Then we do start next transaction (also SNAPSHOT; its name = 'tx1b') and obtain value of RDB$GET_CONTEXT('SYSTEM', 'SNAPSHOT_NUMBER').
  Also, in this second 'tx1b' we add one more record into table with value = -1 using autonomous transaction --> BOTH records should be seen
  in another attachment that will be started after this moment.
  But if this (new) attachment will start Tx with requirement to use snapshot that was for Tx1a then it must see only FIRST record with value=-2.

  We launch then ISQL for establish another transaction and make it perform two transactions:
  1) 'set transaction snapshot' --> must extract both records from test table
  === vs ===
  2) 'set transaction snapshot at number %(snap_num)s' --> must extract only FIRST record with value = -2.

  This is done TWO times: when based snapshot is KNOWN (i.e. tx1b is alive) and after tx1b is committed and base is unknown.
  Second ISQL launch must issue error:
    Statement failed, SQLSTATE = 0B000
    Transaction's base snapshot number does not exist
JIRA:        CORE-6018
FBTEST:      bugs.core_6018
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

init_script = """
    recreate table tsn (sn int);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout_a = """
    MSG                          SET_TX_SNAPSHOT_WITHOUT_NUM
    ============================ ===========================
    Tx base snapshot: yet exists                          -2
    Tx base snapshot: yet exists                          -1
    Records affected: 2
    MSG                          SET_TX_SNAPSHOT_AT_NUMBER_N
    ============================ ===========================
    Tx base snapshot: yet exists                          -2
    Records affected: 1
"""

expected_stdout_b = """
    MSG                               SET_TX_SNAPSHOT_WITHOUT_NUM
    ================================= ===========================
    Tx base snapshot: does not exists                          -2
    Tx base snapshot: does not exists                          -1
    Records affected: 2
    MSG                               SET_TX_SNAPSHOT_AT_NUMBER_N
    ================================= ===========================
    Tx base snapshot: does not exists                          -2
    Tx base snapshot: does not exists                          -1
    Records affected: 2
"""

expected_stderr = """
    Statement failed, SQLSTATE = 0B000
    Transaction's base snapshot number does not exist
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    custom_tpb = tpb(isolation=Isolation.CONCURRENCY, lock_timeout=0)
    with act.db.connect() as con1:
        tx1a = con1.transaction_manager(custom_tpb)
        tx1a.begin()
        cur1 = tx1a.cursor()
        cur1.execute('insert into tsn (sn) values( -2 )')
        tx1a.commit()
        #
        sql_get_sn = """
            execute block returns(o_sn bigint) as
            begin
                o_sn = RDB$GET_CONTEXT('SYSTEM', 'SNAPSHOT_NUMBER');
                suspend;

                in autonomous transaction do
                insert into tsn(sn) values( -1 );
            end
"""
        tx1b = con1.transaction_manager(custom_tpb)
        cur1 = tx1b.cursor()
        snap_num = cur1.execute(sql_get_sn).fetchone()[0]
        #
        for msg, expect_out, expect_err in [('yet exists', expected_stdout_a, ''),
                                            ('does not exists', expected_stdout_b, expected_stderr)]:
            sql_chk_sn = f"""
                -- NB!! looks strange but it seems that this 'SET BAIL ON' does not work here because
                -- both records will be extracted in any case. // todo later: check it!
                --set bail on;
                set count on;
                commit;
                set transaction snapshot;
                select 'Tx base snapshot: {msg}' as msg, t.sn as set_tx_snapshot_without_num from tsn t order by sn;
                commit;
                set transaction snapshot at number {snap_num};
                select 'Tx base snapshot: {msg}' as msg, t.sn as set_tx_snapshot_at_number_N from tsn t order by sn;
                commit;
                quit;
                """
            act.reset()
            act.expected_stdout = expect_out
            act.expected_stderr = expect_err
            act.isql(switches=['-q'], input=sql_chk_sn)
            if tx1b.is_active():
                tx1b.commit()
            assert act.clean_stdout == act.clean_expected_stdout
