#coding:utf-8

"""
ID:          issue-3562
ISSUE:       3562
TITLE:       page 0 is of wrong type (expected 6, found 1)
DESCRIPTION:
JIRA:        CORE-3188
FBTEST:      bugs.core_3188
"""

import pytest
from difflib import unified_diff
from firebird.qa import *

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.connect_server() as srv:
        srv.info.get_log()
        log_before = srv.readlines()
        with act.db.connect() as con1, act.db.connect() as con2:
            c1 = con1.cursor()
            c1.execute("create table test(id int primary key)")
            con1.commit()
            #
            c2 = con2.cursor()
            c2.execute('drop table test')
            con2.commit()
        srv.info.get_log()
        log_after = srv.readlines()
    assert list(unified_diff(log_before, log_after)) == []
