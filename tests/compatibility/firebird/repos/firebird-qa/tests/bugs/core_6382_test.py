#coding:utf-8

"""
ID:          issue-6621
ISSUE:       6621
TITLE:       Triggers accessing a table prevent concurrent DDL command from dropping that table
DESCRIPTION:
  Test creates two tables which are linked by master-detail relationship.
  We add one record into the main table and then update it with issuing further COMMIT.
  After this we try to DROP this table in another connect.

  If this connect started WAIT transaction (i.e. without lock timeout) then it can hang forever if case of
  regression of this fix. Because of this, we change its waiting mode by adding lock_timeout parameter to
  TPB and set it to 1 second.

  BEFORE fix this lead to:
    DatabaseError: / Error while commiting transaction: / - SQLCODE: -901
    - lock time-out on wait transaction / - unsuccessful metadata update
    - object TABLE "T_DETL" is in use / -901 / 335544510

  AFTER fix this DROP TABLE statement must pass without any error.
JIRA:        CORE-6382
FBTEST:      bugs.core_6382
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

init_script = """
    recreate table t_detl(id int primary key using index t_detl_pk, pid int);
    recreate table t_main(id int primary key using index t_main_pk);
    alter table t_detl add constraint t_detl_fk foreign key(pid) references t_main(id) on update cascade using index t_detl_fk;
    commit;
    insert into t_main values(123);
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db')

expected_stdout = """
    None
    Passed.
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):
    #  # NB: adding this timeout does NOT change WAIT-nature of transaction as it is considered by engine.
    #  # (in other words: such transaction will not became 'no wait' which must not be used in this test):
    custom_tpb = tpb(isolation=Isolation.SNAPSHOT, lock_timeout=5)
    #
    with act.db.connect() as con1, act.db.connect() as con2:
        con1.main_transaction.default_tpb = custom_tpb
        con2.main_transaction.default_tpb = custom_tpb
        #
        con1.execute_immediate('update t_main set id=-id')
        con1.commit()

        con2.execute_immediate('drop table t_detl')
        con2.commit()

        cur = con2.cursor()
        cur.execute("select r.rdb$relation_name from rdb$database d left join rdb$relations r on r.rdb$relation_name = upper('t_detl')")
        for r in cur:
            print(r[0])
    print('Passed.')
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
