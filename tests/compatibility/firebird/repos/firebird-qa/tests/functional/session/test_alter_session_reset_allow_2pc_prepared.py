#coding:utf-8

"""
ID:          session.alter-session-reset-allow-2pc-prepared
ISSUE:       6093
TITLE:       ALTER SESSION RESET: do NOT raise error if prepared 2PC transactions present
DESCRIPTION:
  Test issue about ALTER SESSION RESET:
  "throw error (isc_ses_reset_err) if any open transaction exist in current conneciton,
   except of ... prepared 2PC transactions which is allowed and ignored by this check"

  We create two databases with table (id int, x int) in each of them.
  Then we create two connections (one per each DB).

  These connections are added to the instance of fdb.ConnectionGroup() in order to have
  ability to use 2PC mechanism.

  In the first connection we start TWO transactions, in the second it is enough to start one.
  Then we do trivial DML in each of these three transactions: insert one row in a table.

  Finally, we run prepare() method in one of pair transactions that belong to 1st connection.
  After this, we must be able to run ALTER SESSION RESET in the *second* Tx of this pair, and
  this statement must NOT raise any error.

  NB! Without prepare() such action must lead to exception:
  "SQLCODE: -901 / Cannot reset user session / There are open transactions (2 active)"

  Output of this test must remain EMPTY.
FBTEST:      functional.session.alter_session_reset_allow_2pc_prepared
JIRA:        CORE-5832

NOTES:
[12.05.2022] pzotov
    Statement 'ALTER SESSION RESET' being issued in one of distributed transactions leads to WARNING that:
    1. always will be seen in the trace log:
        2022-05-12T20:45:47.8460 (5716:00000000011B19C0) WARNING AT JStatement::execute#
            C:/TEMP/PYTEST.../TMP_2PC_A.FDB (ATT_10, SYSDBA:NONE, NONE, TCPv6:::1/54604)
            C:/python3x/python.exe:3124
        335545208 : Session was reset with warning(s)
        335545209 : Transaction is rolled back due to session reset, all changes are lost

    2. will appear on console:
        C:/python3x/lib/site-packages/firebird/driver/interfaces.py:703: FirebirdWarning: Session was reset with warning(s)
        -Transaction is rolled back due to session reset, all changes are lost

    Because of this, 'with pytest.warns(FirebirdWarning, ...' is used to suppress appearance of this warning in the pytest output.

    NB-1. We have to create TWO DistributedTransactionManager instance for the SAME connections in order to start two transactions
          within any of connections involved into distributed work.
    NB-2. DistributedTransactionManager has associated resources, so it's better to use "with" statement to ensure that resources
          are always properly released.
    NB-3. This test does not have any expected stdout & stderr (they are empty strings). It's better to remove them including
          unnecessary assert - for clarity. The real test is the check for warning, not for stdout/stderr.
    See letters from pcisar, 12.05.2022 11:28; 13.05.2022 20:59.

    Checked on 4.0.2.2692, 5.0.0.489.
"""

import pytest
from firebird.qa import *
from firebird.driver import DistributedTransactionManager, tpb, Isolation, FirebirdWarning

init_script = """
    create table test(id int, x int, s varchar(10), constraint test_pk primary key(id) using index test_pk);
"""
db_a = db_factory(filename='tmp_2pc_a.fdb', init = init_script)
db_b = db_factory(filename='tmp_2pc_b.fdb', init = init_script)

act = python_act('db_a')

@pytest.mark.version('>=4.0')
def test_1(act: Action, db_b: Database):
    
    til1 = tpb(Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout = 111)
    til2 = tpb(Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout = 112)
    til3 = tpb(Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout = 222)
    
    with act.db.connect() as con1, db_b.connect() as con2:
        with DistributedTransactionManager((con1,con2)) as dt1, DistributedTransactionManager((con1,con2)) as dt2:
            cur1a=dt1.cursor(con1)
            cur1b=dt2.cursor(con1)
            cur2=dt2.cursor(con2)

            cur1a.execute( "insert into test(id, x, s) values( ?, ?, ? )", (1, 111, 'db_a') )
            cur1b.execute( "insert into test(id, x,s ) values( ?, ?, ? )", (3, 333, 'db_a') )
            cur2.execute(  "insert into test(id, x, s) values( ?, ?, ? )", (2, 222, 'db_b') )

            # NOTE: following call of dt1.prepare() is necessary!
            # Otherwise we get (on attempt to execute 'alter session reset'):
            #  "cannot disconnect database with open transactions (2 active)"
            # followed by: "connection shutdown / -Killed by database administrator"
            #
            dt1.prepare()

            with pytest.warns(FirebirdWarning, match='.*due to session reset.*'):
                # NB: argument match='.*Session was reset.*' also can be used
                cur1b.execute( "alter session reset" )

            dt1.rollback()
            dt2.rollback()
