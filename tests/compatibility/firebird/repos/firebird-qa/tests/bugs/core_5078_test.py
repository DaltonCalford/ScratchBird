#coding:utf-8

"""
ID:          issue-5365
ISSUE:       5365
TITLE:       "Invalid BLOB ID" error
DESCRIPTION:
JIRA:        CORE-5078
FBTEST:      bugs.core_5078
"""

import pytest
from pathlib import Path
import zipfile
from firebird.qa import *

db = db_factory()

act = python_act('db')

fbk_file = temp_file('tmp_core_5078.fbk')
fdb_file = temp_file('tmp_core_5078.fdb')

@pytest.mark.version('>=2.5.6')
def test_1(act: Action, fbk_file: Path, fdb_file: Path):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'core_5078.zip',
                    at='tmp_core_5078.fbk')
    fbk_file.write_bytes(zipped_fbk_file.read_bytes())
    with act.connect_server() as srv:
        srv.database.restore(database=fdb_file, backup=fbk_file)
        srv.wait()
    # This should execute without errors
    act.isql(switches=[str(fdb_file)], input='set list on; select * from do_changeTxStatus;',
               connect_db=False)
