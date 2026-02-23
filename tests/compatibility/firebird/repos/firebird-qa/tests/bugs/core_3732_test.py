#coding:utf-8

"""
ID:          issue-4078
ISSUE:       4078
TITLE:       Segfault when closing attachment to database
DESCRIPTION:
JIRA:        CORE-3732
FBTEST:      bugs.core_3732
"""

import pytest
from difflib import unified_diff
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('STATEMENT FAILED, SQLSTATE = HY000', ''),
                                        ('RECORD NOT FOUND FOR USER: TMP\\$C3732', ''),
                                        ('AFTER LINE.*', '')])

test_script = """
    create role REPL_ADMIN;
    create user tmp$c3732 password '12345';
    grant repl_admin to tmp$c3732;
    revoke all on all from tmp$c3732;
    drop user tmp$c3732;
    drop role REPL_ADMIN;
    exit;
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    with act.connect_server() as srv:
        srv.info.get_log()
        log_before = srv.readlines()
    act.isql(switches=['-q'], input=test_script)
    with act.connect_server() as srv:
        srv.info.get_log()
        log_after = srv.readlines()
    assert list(unified_diff(log_before, log_after)) == []

