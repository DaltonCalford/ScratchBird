#coding:utf-8

"""
ID:          dml.skip_locked
TITLE:       Basic check of SKIP LOCKED
DESCRIPTION:
    Trivial test to check SKIP LOCKED functionality on all kinds of transaction isolation level.
    More complex cases see in gh_7350_test.py.
NOTES:
    [26.02.2025] pzotov
        Commit that introduced this feature (5.0.0.811, 29-oct-2022):
        https://github.com/FirebirdSQL/firebird/commit/5cc8a8f7fd27d72d5ca6f19eb691e93f2404ddd1
    [06.07.2025] pzotov
        Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
        Checked on 6.0.0.930; 5.0.3.1668.
"""
from firebird.driver import tpb, Isolation, DatabaseError 

import pytest
from firebird.qa import *

init_script = \
f'''
    set bail on;
    recreate table test(id int primary key, f01 int);
    commit;
    insert into test(id, f01) select row_number()over(), 0 from rdb$types rows 10;
    commit;
'''

db = db_factory(init = init_script )
act = python_act('db')

@pytest.mark.version('>=5.0')
def test_1(act: Action, capsys):

    tx_isol_lst = [ 
                    Isolation.SERIALIZABLE,
                    Isolation.SNAPSHOT,
                    Isolation.READ_COMMITTED_NO_RECORD_VERSION,
                    Isolation.READ_COMMITTED_RECORD_VERSION,
                  ]
    if act.is_version('>=4'):
       tx_isol_lst.append(Isolation.READ_COMMITTED_READ_CONSISTENCY)

    with act.db.connect() as con_locker, act.db.connect() as con_worker:
        con_locker.execute_immediate('update test set f01 = 1 where id in (1,5,9)')
        for x_isol in tx_isol_lst:
            custom_tpb = tpb(isolation = x_isol, lock_timeout = 0)
            print(x_isol.name)
            tx_worker = con_worker.transaction_manager(custom_tpb)
            tx_worker.begin()
            cur = tx_worker.cursor()
            cur.execute('select id from test with lock skip locked')
            try:
                for r in cur:
                    print('TIL:',x_isol.name, 'ID:', r[0])
            except DatabaseError as e:
                print(e)
                # E firebird.driver.types.DatabaseError: lock conflict on no wait transaction
                # E -Acquire lock for relation (TEST) failed

            cur.close()
            tx_worker.rollback()

        SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
        TEST_TABLE_NAME = 'TEST' if act.is_version('<6') else  f'{SQL_SCHEMA_PREFIX}"TEST"'
        act.expected_stdout = f"""
            SERIALIZABLE
            lock conflict on no wait transaction
            -Acquire lock for relation ({TEST_TABLE_NAME}) failed
            
            SNAPSHOT
            TIL: SNAPSHOT ID: 2
            TIL: SNAPSHOT ID: 3
            TIL: SNAPSHOT ID: 4
            TIL: SNAPSHOT ID: 6
            TIL: SNAPSHOT ID: 7
            TIL: SNAPSHOT ID: 8
            TIL: SNAPSHOT ID: 10
            
            READ_COMMITTED_NO_RECORD_VERSION
            TIL: READ_COMMITTED_NO_RECORD_VERSION ID: 2
            TIL: READ_COMMITTED_NO_RECORD_VERSION ID: 3
            TIL: READ_COMMITTED_NO_RECORD_VERSION ID: 4
            TIL: READ_COMMITTED_NO_RECORD_VERSION ID: 6
            TIL: READ_COMMITTED_NO_RECORD_VERSION ID: 7
            TIL: READ_COMMITTED_NO_RECORD_VERSION ID: 8
            TIL: READ_COMMITTED_NO_RECORD_VERSION ID: 10
            
            READ_COMMITTED_RECORD_VERSION
            TIL: READ_COMMITTED_RECORD_VERSION ID: 2
            TIL: READ_COMMITTED_RECORD_VERSION ID: 3
            TIL: READ_COMMITTED_RECORD_VERSION ID: 4
            TIL: READ_COMMITTED_RECORD_VERSION ID: 6
            TIL: READ_COMMITTED_RECORD_VERSION ID: 7
            TIL: READ_COMMITTED_RECORD_VERSION ID: 8
            TIL: READ_COMMITTED_RECORD_VERSION ID: 10
            
            READ_COMMITTED_READ_CONSISTENCY
            TIL: READ_COMMITTED_READ_CONSISTENCY ID: 2
            TIL: READ_COMMITTED_READ_CONSISTENCY ID: 3
            TIL: READ_COMMITTED_READ_CONSISTENCY ID: 4
            TIL: READ_COMMITTED_READ_CONSISTENCY ID: 6
            TIL: READ_COMMITTED_READ_CONSISTENCY ID: 7
            TIL: READ_COMMITTED_READ_CONSISTENCY ID: 8
            TIL: READ_COMMITTED_READ_CONSISTENCY ID: 10
        """
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
