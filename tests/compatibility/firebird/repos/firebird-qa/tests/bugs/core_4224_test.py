#coding:utf-8

"""
ID:          issue-4548
ISSUE:       4548
TITLE:       Database replace through services API fails
DESCRIPTION:
JIRA:        CORE-4224
FBTEST:      bugs.core_4224
"""

import pytest
import os
from io import BytesIO
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action):
    src_timestamp1 = os.path.getmtime(act.db.db_path)
    backup = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(database=act.db.db_path, backup_stream=backup,
                                   flags=SrvRestoreFlag.REPLACE)
    src_timestamp2 = os.path.getmtime(act.db.db_path)
    assert src_timestamp2 - src_timestamp1 > 0


