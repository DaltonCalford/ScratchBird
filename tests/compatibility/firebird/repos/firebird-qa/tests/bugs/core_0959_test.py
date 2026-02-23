#coding:utf-8

"""
ID:          issue-1362
ISSUE:       1362
TITLE:       GSTAT does not work using the localhost connection string (Permission denied)
DESCRIPTION:
JIRA:        CORE-959
FBTEST:      bugs.core_0959
NOTES:
    [02.12.2025] pzotov
    Removed unneeded code. It is enough for this test just to check that gstat STDERR is empty
    when DB is specified with 'localhost:' prefix.
"""

import locale
import pytest
from firebird.qa import *
import time

db = db_factory()
act = python_act('db')

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    assert 'localhost:' in act.db.dsn
    act.gstat(switches=['-d', '-i', '-r' ], io_enc = locale.getpreferredencoding())
    assert act.clean_stderr == ''
