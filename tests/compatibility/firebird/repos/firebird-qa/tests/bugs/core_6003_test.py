#coding:utf-8

"""
ID:          issue-6253
ISSUE:       6253
TITLE:       RDB$GET_TRANSACTION_CN works different in Super and Classic
DESCRIPTION:
  NB-1: bug can be reproduced using ISQL but it must be lacunhed with '-n' command switch.
  NB-2: connection-1 (which finally asks value of rdb$get_transaction_cn(<Tx2>)) must start Tx1
        *BEFORE* connection-2 will start his Tx2.
JIRA:        CORE-6003
FBTEST:      bugs.core_6003
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    custom_tpb = tpb(isolation=Isolation.CONCURRENCY)
    with act.db.connect() as con1:
        tx1a = con1.transaction_manager(custom_tpb)
        tx1a.begin() # tx1a must be started BEFORE tx2a!
        with act.db.connect() as con2:
            tx2a = con2.transaction_manager(custom_tpb)
            tx2a.begin()
            #
            cur2 = tx2a.cursor()
            trn2 = cur2.execute('select current_transaction from rdb$database').fetchone()[0]
            tx2a.commit()
            #
            cur1 = tx1a.cursor()
            cur1.execute(f"select 'Result is ' || iif( rdb$get_transaction_cn({trn2}) is null, 'INCORRECT: NULL.', 'expected: NOT null.') from rdb$database")
            assert cur1.fetchone()[0] == 'Result is expected: NOT null.'
