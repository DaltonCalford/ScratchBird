#coding:utf-8

"""
ID:          issue-2170
ISSUE:       2170
TITLE:       Expression index can be created while doing inserts into table
DESCRIPTION:
JIRA:        CORE-1746
FBTEST:      bugs.core_1746
NOTES:
    [15.01.2022] pcisar
        This test may FAIL when run on slow machine (like VM), or fast one (Windows 10 with SSD)
    [19.09.2022] pzotov
        Fully reimplemented. Issue described in the ticket can be easy reproduced WITHOUT any concurrent workload.
        We can create table with single column and two connections. First of them just inserts one record and
        second attempts to create index.
        On 2.5.3.26780 and 3.0.0.32483 statement 'create index' will pass (and this must be considered as problem).
        On 2.5.27020 and 3.0.1 such attempt leads to exception "-901 / object ... in use" - and this is expected.
        See also core_4386_test.py.
        Checked on 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.730 (SS/CS)
    [25.06.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from pathlib import Path

from firebird.qa import *
from firebird.driver import tpb, Isolation

db = db_factory(init='create table test(id int);')

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):

    tx_isol_lst = [ Isolation.READ_COMMITTED_NO_RECORD_VERSION,
                    Isolation.READ_COMMITTED_RECORD_VERSION,
                    Isolation.SNAPSHOT,
                    Isolation.SERIALIZABLE,
                  ]
    if act.is_version('>=4'):
       tx_isol_lst.append(Isolation.READ_COMMITTED_READ_CONSISTENCY)

    # for any isolation mode attempt to run DDL for table that is in use by another Tx must fail
    # with the same error message. We check all possible Tx isolation modes for that:
    for x_isol in tx_isol_lst:
    
        custom_tpb = tpb(isolation = x_isol, lock_timeout = 0)

        with act.db.connect() as con1, act.db.connect() as con2:
            tx1 = con1.transaction_manager(custom_tpb)
            tx2 = con2.transaction_manager(custom_tpb)
            tx1.begin()
            tx2.begin()
            with tx1.cursor() as cur1, tx2.cursor() as cur2:
                try:
                    cur1.execute( "insert into test(id) values(1)" )
                    cur2.execute('create index test_calc_idx on test computed by(id*id)')
                    tx2.commit()
                except Exception as e:
                    print(e.__str__())
                    print(e.gds_codes)

        if act.is_version('<6'):
            expected_out = """
                lock conflict on no wait transaction
                -unsuccessful metadata update
                -object TABLE "TEST" is in use
                (335544345, 335544351, 335544453)
            """
        else:
            expected_out = """
                lock conflict on no wait transaction
                -unsuccessful metadata update
                -object TABLE "PUBLIC"."TEST" is in use
                (335544345, 335544351, 335544453)
            """

        act.expected_stdout = expected_out
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
