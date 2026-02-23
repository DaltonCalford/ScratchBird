#coding:utf-8

"""
ID:          issue-3405
ISSUE:       3405
TITLE:       Error "no current record for fetch operation" after ALTER VIEW
DESCRIPTION:
JIRA:        CORE-3024
FBTEST:      bugs.core_3024
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('-', '')])

expected_stdout = """
    A           B           C
    1           2           3
"""

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    with act.db.connect() as att1, act.db.connect() as att2:
        trn1 = att1.transaction_manager()
        cur1 = trn1.cursor()
        cur1.execute("create table t(a int, b int, c int)")   # att_12, tra_4
        cur1.execute("create view v as select a,b from t")
        trn1.commit()
        cur1.execute("insert into t values(1,2,3)")           # att_12, tra_5
        cur1.execute("select * from v")
        trn1.commit()
        trn2 = att2.transaction_manager()
        cur2 = trn2.cursor()
        cur2.execute("select * from v")                       # att_13, tra_7
        trn2.commit()
        cur1.execute("alter view v as select a, b, c from t") # att-12, tra_8
        trn1.commit()
        cur2.execute("select * from v")                       # att_13, tra_9
        act.print_data(cur2)
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
