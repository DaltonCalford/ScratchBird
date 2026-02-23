#coding:utf-8

"""
ID:          issue-2224
ISSUE:       2224
TITLE:       Bugcheck 165 (cannot find tip page)
DESCRIPTION:
  Classic and SuperClassic only

  Bug can be reproduced only when FIRST of ISQL sessions is lacunhed with '-n' switch.
  Second ISQL must be started *WITHOUT* this switch!
  Absence of '-n' means that ISQL always starts two transactions (first for DML and second for DDL)
  and they both are committed at the same time for each executed statement.
  Because of this, we use here two transaction for second connection and, furthermore, we use
  the same isolation levels for them, namely: SNAPSHOT for DML and READ COMMITTED for DDL.
  This is done by using custom TPB objects with apropriate properties - see 'dml_tpb' and 'ddl_tpb'.

  Confirmed bug on 3.0.4.32972 (build date: 11-may-2018), got:
    SQLCODE: -902 / - ... consistency check (can't continue after bugcheck) / -902 / 335544333
  firebird.log will contain after this:
    internal Firebird consistency check (cannot find tip page (165), file: tra.cpp line: 2331)
JIRA:        CORE-5935
FBTEST:      bugs.core_5935
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3.0.5')
def test_1(act: Action):
    # CONCURRENCY | WAIT | READ_WRITE
    dml_tpb = tpb(isolation=Isolation.CONCURRENCY)
    # READ_COMMITTED | NO_REC_VERSION | WAIT | READ_WRITE
    ddl_tpb = tpb(isolation=Isolation.READ_COMMITTED_NO_RECORD_VERSION)
    #
    with act.db.connect() as con:
        con.execute_immediate('recreate table a (id int)')
        con.commit()
        con.execute_immediate('create index idx_a on a(id)')
        con.commit()
        sql = """
        create or alter procedure p_gen_tx(n int) as
            declare i int = 0;
        begin
            while (i < n) do
              in autonomous transaction do
                i = i + 1;
        end
        """
        con.execute_immediate(sql)
        con.commit()
    # Test
    con = act.db.connect()
    tx1a = con.transaction_manager(dml_tpb)
    tx1a.begin()
    cur1 = tx1a.cursor()
    cur1.execute('delete from a')
    tx1a.commit()
    #
    tx1a.begin()
    cur1.execute("select current_transaction, rdb$get_context('SYSTEM', 'ISOLATION_LEVEL') from rdb$database")
    cur1.fetchall()
    # ---
    con2 = act.db.connect()
    tx2a = con2.transaction_manager(dml_tpb)
    tx2b = con2.transaction_manager(ddl_tpb)
    #
    tx2a.begin()
    tx2b.begin()
    cur2 = tx2a.cursor()
    cur2.callproc('p_gen_tx', [33000])
    tx2a.commit()
    tx2b.commit()
    #
    tx2a.begin()
    tx2b.begin()
    cur2.execute('insert into a (id) values (?)', [tx2a.info.id])
    tx2a.commit()
    tx2b.commit()
    #
    tx2a.begin()
    tx2b.begin()
    cur2.execute('set statistics index idx_a')
    tx2a.commit()
    tx2b.commit()
    #
    tx2a.begin()
    tx2b.begin()
    cur2.execute('select rdb$index_name, rdb$record_version from rdb$indices where rdb$relation_name = ?', ['A'])
    cur2.fetchall()
    cur2.execute('select id from a where id > ?', [0])
    cur2.fetchall()
    tx2a.commit()
    tx2b.commit()
    #
    tx2a.begin()
    tx2b.begin()
    cur2 = tx2a.cursor()
    cur2.callproc('p_gen_tx', [33000])
    tx2a.commit()
    tx2b.commit()
    # ---
    tx1a.commit()
    # ---
    tx2a.begin()
    tx2b.begin()
    cur2.execute('select id from a where id > ?', [0])
    cur2.fetchall()
    # ---
    tx1a.begin()
    cur1.execute('select id from a where id > ?', [0])
    cur1.fetchall()
    #
    cur1.close()
    tx1a.rollback()
    con.close()
    #
    cur2.close()
    tx2a.rollback()
    tx2b.rollback()
    con2.close()
    # Passed
