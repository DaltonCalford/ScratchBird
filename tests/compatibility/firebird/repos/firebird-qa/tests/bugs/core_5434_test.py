#coding:utf-8

"""
ID:          issue-5706
ISSUE:       5706
TITLE:       Read-only transactions in SuperServer could avoid immediate write of Header and TIP pages after change
DESCRIPTION:
  If current FB arch is SuperServer then we:
    1. We make 'snapshot' of mon$io_stats.mon$page_writes value before test and then launch plently transactions (e.g, 50)
       in READ-ONLY mode. All of them then are immediately committed, w/o any actions.
    2. After this we take 2nd 'snapshot' of mon$io_stats.mon$page_writes and compare it with 1st one.
    3. Difference of 'mon$page_writes' values should be 1 (One).
  Otherwise (SC/CS) we defer checking because improvement currently not implemented for these modes.
JIRA:        CORE-5434
FBTEST:      bugs.core_5434
"""

import pytest
from firebird.qa import *
from firebird.driver import TraAccessMode, TPB

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3.0.2')
def test_1(act: Action):
    tpb = TPB(access_mode=TraAccessMode.READ).get_buffer()
    sql = 'select mon$page_writes from mon$io_stats where mon$stat_group=0'
    if act.get_server_architecture() == 'SuperServer':
        with act.db.connect() as con:
            with act.db.connect() as con2:
                cur = con.cursor()
                page_writes_before_test = cur.execute(sql).fetchone()[0]
                con.commit()
                #
                ta = []
                for i in range(50):
                    tra = con2.transaction_manager(default_tpb=tpb)
                    tra.begin()
                    ta.append(tra)
                for tra in ta:
                    tra.rollback()
                #
                cur = con.cursor()
                page_writes_after_test = cur.execute(sql).fetchone()[0]
                con.commit()
                #
                assert page_writes_after_test - page_writes_before_test == 1
    else:
        pytest.skip('Applies only to SuperServer')
