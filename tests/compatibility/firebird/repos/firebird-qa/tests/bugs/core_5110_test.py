#coding:utf-8

"""
ID:          issue-5394
ISSUE:       5394
TITLE:       False PK/FK violation could be reported when attachment used isc_dpb_no_garbage_collect flag
DESCRIPTION:
  Fix relates only such transactions that are in the state 'dead', see: https://sourceforge.net/p/firebird/code/62965
  This mean that such Tx should did sufficiently big changes _before_ inserting key into table with PK/UK.
  Number of these changes should force Tx-1 be rolled back via TIP rather than undo+commit.
  Simple experiment shows that threshold is near 80'000 rows being inserted into the table with single
  text field of length = 50 characters. This test inserts 120'000 rows for ensuring that rollback will be
  done via TIP.
JIRA:        CORE-5110
FBTEST:      bugs.core_5110
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

init_script = """
    recreate table test( id int, who varchar(5), constraint test_id_unq unique(id) using index test_id_unq );
    recreate table tbig( s varchar(50) );
"""

db = db_factory(init=init_script)

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    custom_tpb = tpb(isolation=Isolation.CONCURRENCY)
    with act.db.connect(no_gc=True) as con:
        tx1 = con.transaction_manager(custom_tpb)
        tx2 = con.transaction_manager(custom_tpb)
        tx1.begin()
        tx2.begin()
        cur1 = tx1.cursor()
        cur2 = tx2.cursor()
        # Test starts here, no exception should occur
        # Tx-1: insert big number of rows.
        cur1.execute("insert into tbig(s) select rpad('', 50, uuid_to_char(gen_uuid())) from rdb$types,rdb$types,(select 1 k from rdb$types rows 2) rows 120000")
        cur1.execute("insert into test(id, who) values(1, 'Tx-1')")
        # Tx-1: rollback via TIP.
        tx1.rollback()
        # Tx-2: insert single test record.
        cur2.execute("insert into test(id, who) values(1, 'Tx-2')")
        tx2.rollback()
