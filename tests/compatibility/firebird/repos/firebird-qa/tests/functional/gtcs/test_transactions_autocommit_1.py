#coding:utf-8

"""
ID:          gtcs.transactions-autocommit-01
TITLE:       AUTO COMMIT must preserve changes that were made by all DML even if ROLLBACK is issued
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/AUTO_COMMIT.1.ESQL.script

  Test creates three tables (test_1, test_2 and test_3) and AI-trigger for one of them (test_1).
  This trigger does INSERTs into test_2 and test_3.

  Then we add record into test_1 thus its trigger add records into test_2 and test_3.
  After this we make transaction ROLLED BACK and check how many records are preserved.
  Expected result: each of three tables must have one record in itself,
  i.e. result looks like we did COMMIT rather than ROLLBACK.

  NB: we use custom TPB with fdb.isc_tpb_autocommit in order to start DML transactions in AUTOCOMMIT=1 mode.
FBTEST:      functional.gtcs.transactions_autocommit_1
"""

import pytest
from firebird.qa import *
from firebird.driver import *

init_script = """
    set bail on;
    recreate table test_1 (x integer);
    recreate table test_2 (x integer);
    recreate table test_3 (x integer);
    commit;
    set term ^;
    create or alter trigger trg_test1_ai for test_1 active after insert position 0 as
    begin
        insert into test_2 values (new.x * 7);
        insert into test_3 values (new.x * 777);
    end ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    mon$auto_commit:	1  
    test_1				777            			  		 	
    test_2          	5439   			
    test_3 				603729 				  		
"""

# functional\fkey\unique\test_select_uf_01.py 
# bugs\core_4840_test.py 

custom_tpb = TPB(lock_timeout = 0, auto_commit=True).get_buffer()

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    with act.db.connect() as con:

        con.begin(custom_tpb)
        with con.cursor() as cur:
            for r in cur.execute('select mon$auto_commit from mon$transactions where mon$transaction_id = current_transaction').fetchall():
                print('mon$auto_commit:', r[0])
        
            cur.execute( 'insert into test_1 values(?)', ( 777,) )

        # Despite this ROLLBACK all *three* records must remain in DB because auto_commit = true:
        con.rollback()

        with con.cursor() as cur:
            for r in cur.execute("select 'test_1' tab_name, x from test_1 union all select 'test_2', x from test_2 union all select 'test_3', x from test_3").fetchall():
                print(r[0], r[1])

    assert act.clean_string(capsys.readouterr().out, act.substitutions) == act.clean_string(expected_stdout, act.substitutions)

