#coding:utf-8

"""
ID:          gtcs.transactions-autocommit-03
TITLE:       Changes within AUTO COMMIT must be cancelled when exception raises in some PROCEDURE
DESCRIPTION:
  Test does the same actions as described in GTCS/tests/AUTO_COMMIT.3.ESQL.script, see:
  https://github.com/FirebirdSQL/fbtcs/commit/166cb8b72a0aad18ef8ece34977d6d87d803616e#diff-69d4c7d7661d57fdf94aaf32a3377c82

  It creates three tables, each with single SMALLINT column (thus max value that we can put in it is 32767).
  Then it creates three procedures which do insert values in 'their' table plus call "next level" SP
  with passing there input value multiplied by 100.
  When sp_ins_1 is called with argument = 3 then sp_ins_2 will insert into test_2 table value = 300
  and sp_ins_3 will insert into test_3 value = 30000.
  This means that we can can NOT call sp_ins_1 with values equal or more than 4 because of numeric overflow exception
  that will be raised in sp_ins_3.

  Test calls sp_ins1 two times: with arg=3 and arg=4. Second time must fail and we check that all three tables contain only
  values which are from 1st call: 3, 300 and 30000.

  NB: we use custom TPB with fdb.isc_tpb_autocommit in order to start DML transactions in AUTOCOMMIT=1 mode.
FBTEST:      functional.gtcs.transactions_autocommit_3
"""

import pytest
from firebird.qa import *
from firebird.driver import *

N_MAX = 3
sql_proc = """
    create table test_%(i)s(x smallint)
    ^
    create procedure sp_ins_%(i)s(a_x bigint) as
    begin
        insert into test_%(i)s(x) values(:a_x);
        if ( %(i)s != %(N_MAX)s ) then
            execute statement ( 'execute procedure sp_ins_%(k)s (?)' ) (:a_x * 100);
    end
    ^
    commit
    ^
"""

init_script = 'set term ^;'
for i in range(N_MAX,0,-1):
    k = i+1
    init_script += sql_proc % locals()

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    mon$auto_commit: 1
    exception occured, gdscodes: (335544321, 335544916, 335544842, 335544842, 335544842)
    10
    3
    300
    30000
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

        with con.cursor() as cur:
            cur.callproc( 'sp_ins_1', (3,) )

            try:
                # this will cause numeric overflow exception in sp_ins_3:
                cur.callproc( 'sp_ins_1', (4,) )
            except DatabaseError as e:
                print('exception occured, gdscodes:', e.gds_codes)

        con.commit()
        with con.cursor() as cur:
            for r in cur.execute('select x from test_1 union all select x from test_2 union all select x from test_3').fetchall():
                print(r[0])

    assert act.clean_string(capsys.readouterr().out, act.substitutions) == act.clean_string(expected_stdout, act.substitutions)

