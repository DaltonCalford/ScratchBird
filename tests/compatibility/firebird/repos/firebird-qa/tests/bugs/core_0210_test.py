#coding:utf-8

"""
ID:          issue-538
ISSUE:       538
TITLE:       CS server crash altering SP in 2 connect
DESCRIPTION:
JIRA:        CORE-210
FBTEST:      bugs.core_0210
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    stm1 = """create or alter procedure sp_test as
    begin
        exit;
    end
    """
    stm2 = """create or alter procedure sp_test as
        declare x int;
    begin
        exit;
    end
    """
    custom_tpb = tpb(isolation=Isolation.CONCURRENCY)
    with act.db.connect() as con1, act.db.connect() as con2:
        con1.begin(custom_tpb)
        cur1 = con1.cursor()
        cur2 = con2.cursor()

        cur1.execute(stm1)
        con1.commit()

        con2.begin(custom_tpb)
        cur2.execute(stm2)
        con2.commit()

        con1.begin(custom_tpb)
        cur1.execute(stm1)
        con1.commit()


