#coding:utf-8

"""
ID:          gtcs.transactions-autocommit-02
TITLE:       Changes within AUTO COMMIT must be cancelled when exception raises in some TRIGGER
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/AUTO_COMMIT.2.ESQL.script

  Test creates three tables (test_1, test_2 and test_3) and AI-trigger for one of them (test_1).
  This trigger does INSERTs into test_2 and test_3.
  For test_3 we create UNIQUE index that will prevent from insertion of duplicates.
  Then we add one record into test_3 with value = 1000.
  Finally, we try to add record into test_1 and after this INSERT its trigger attempts to add records,
  into test_2 and test_3. The latter will fail because of UK violation (we try to insert apropriate value
  into test-1 in order this exception be raised).
  Expected result: NONE of just performed INSERTS must be saved in DB. The only existing record must be
  in the table test_3 that we added there on initial phase.

  NB: we use custom TPB with fdb.isc_tpb_autocommit in order to start DML transactions in AUTOCOMMIT=1 mode.
FBTEST:      functional.gtcs.transactions_autocommit_2
"""

import pytest
from firebird.qa import *
from firebird.driver import *


init_script = """
    set bail on;
    recreate table test_1 (x integer);
    recreate table test_2 (x integer);
    recreate table test_3 (x integer);
    create unique index test_3_x_uniq on test_3 (x);
    commit;
    set term ^;
    create or alter trigger trg_test1_ai for test_1 active after insert position 0 as
    begin
        insert into test_2 values (new.x * 10);
        insert into test_3 values (new.x * 100);
    end ^
    set term ;^

    insert into test_3 values (1000);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    mon$auto_commit: 1
    exception occured, gdscodes: (335544349, 335545072, 335544842)
    test_3 1000
"""

custom_tpb = TPB(lock_timeout = 0, auto_commit=True).get_buffer()

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    with act.db.connect() as con:

        con.begin(custom_tpb)
        with con.cursor() as cur:
            for r in cur.execute('select mon$auto_commit from mon$transactions where mon$transaction_id = current_transaction').fetchall():
                print('mon$auto_commit:', r[0])
        
            try:
                cur.execute( 'insert into test_1 values(?)', (10,) ) # this leads to PK/UK violation in the table 'test_3'
            except DatabaseError as e:
                print('exception occured, gdscodes:', e.gds_codes)

        con.commit()

        with con.cursor() as cur:
            for r in cur.execute("select 'test_1' tab_name, x from test_1 union all select 'test_2', x from test_2 union all select 'test_3', x from test_3").fetchall():
                print(r[0], r[1])


    assert act.clean_string(capsys.readouterr().out, act.substitutions) == act.clean_string(expected_stdout, act.substitutions)
