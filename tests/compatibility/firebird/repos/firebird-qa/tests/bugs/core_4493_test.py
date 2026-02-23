#coding:utf-8

"""
ID:          issue-4812
ISSUE:       4812
TITLE:       Index not found for RECREATE TRIGGER
DESCRIPTION:
JIRA:        CORE-4493
FBTEST:      bugs.core_4493
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table mvt(id int primary key, ac int, amount numeric(12,2));
    recreate table account(id int primary key, balance numeric(12,2));
    commit;

    set term ^;
    recreate trigger tai_mvt active after insert or update position 1 on mvt as
    begin
        update account a set a.balance = a.balance + new.amount
        where a.id = new.ac;
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()
