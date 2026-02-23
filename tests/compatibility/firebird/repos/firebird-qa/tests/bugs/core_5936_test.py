#coding:utf-8

"""
ID:          issue-6192
ISSUE:       6192
TITLE:       Firebird server segfaults in the end of database backup
DESCRIPTION: Firebird crashes, related to Bugcheck 165 (cannot find tip page)
  NB. Ticket title: "Firebird server segfaults in the end of database backup" - has nothing
  to the actual reason of segfault.
  Confirmed crash on:

  Got in firebird.log:
    Access violation.
        The code attempted to access a virtual
        address without privilege to do so.
    Operating system call ReleaseSemaphore failed. Error code 6

  NOTE-1: custom transaction TPB required for this ticket: fdb.isc_tpb_concurrency, fdb.isc_tpb_wait

  NOTE-2: current title of ticket ("Firebird server segfaults in the end of database backup") has no relation to backup action.
  I left here only first two words from it :-)

  Bug was fixed by one-line change in FB source, see:
  https://github.com/FirebirdSQL/firebird/commit/676a52625c074ef15e197e7b7538755195a66905
JIRA:        CORE-5936
FBTEST:      bugs.core_5936
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

db = db_factory()

act = python_act('db')

GEN_ROWS = 17000 # ----- minimal value needed for making FB to crash

ddl_script = """
    create table a (id int);
    create index idx_a on a computed by (id);
    commit;
    set term ^;
    create procedure p_gen_tx(n int) as
        declare i int = 0;
    begin
        while (i < n) do
            in autonomous transaction do
                i = i + 1;
    end ^
    set term ;^
    commit;
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.isql(switches=[], input=ddl_script)
    custom_tpb = tpb(isolation=Isolation.CONCURRENCY)
    #
    with act.db.connect() as con1:
        tx1 = con1.transaction_manager(custom_tpb)
        tx1.begin()
        cur1 = tx1.cursor()
        cur1.execute( "select current_transaction, rdb$get_context('SYSTEM', 'ISOLATION_LEVEL') from rdb$database" )
        cur1.fetchall()
        with act.db.connect() as con2:
            tx2 = con2.transaction_manager(custom_tpb)
            tx2.begin()
            cur2 = tx2.cursor()
            cur2.callproc('p_gen_tx', [GEN_ROWS])
            tx2.commit()
            #
            tx2.begin()
            cur2.execute('insert into a values (current_transaction)')
            tx2.commit()
            #
            tx2.begin()
            cur2.execute('set statistics index idx_a')
            tx2.commit()
            #
            tx2.begin()
            cur2.execute('select * from a where id > 0')
            cur2.fetchall()
            tx2.commit()
            #
            tx2.begin()
            cur2.callproc('p_gen_tx', [GEN_ROWS])
            tx2.commit()
            # ---
            tx1.commit()
            cur1.execute('select * from a where id > 0')
            cur1.fetchall() # WI-V2.5.8.27089 crashed here
            tx1.commit()
