#coding:utf-8

"""
ID:          issue-5985
ISSUE:       5985
TITLE:       FB >= 3 crashes when restoring backup made by FB 2.5
DESCRIPTION:
  This test also present in GTCS list, see it here:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/SV_HIDDEN_VAR_2_5.script
JIRA:        CORE-5719
FBTEST:      bugs.core_5719
"""

import pytest
import zipfile
from difflib import unified_diff
from pathlib import Path
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

db = db_factory()

act = python_act('db')

fbk_file = temp_file('core5719-ods-11_2.fbk')
fdb_file = temp_file('check_restored_5719.fdb')

@pytest.mark.version('>=3.0')
def test_1(act: Action, fbk_file: Path, fdb_file: Path):
    zipped_fbk_file = zipfile.Path(act.files_dir / 'core_5719-ods-11_2.zip',
                                   at='core5719-ods-11_2.fbk')
    fbk_file.write_bytes(zipped_fbk_file.read_bytes())
    log_before = act.get_firebird_log()
    #
    with act.connect_server() as srv:
        srv.database.restore(backup=fbk_file, database=fdb_file,
                             flags=SrvRestoreFlag.REPLACE, verbose=True)
        restore_err = [line for line in srv if 'ERROR' in line.upper()]
        log_after = act.get_firebird_log()
        srv.database.validate(database=fdb_file)
        validate_err = [line for line in srv if 'ERROR' in line.upper()]
    #
    assert restore_err == []
    assert validate_err == []
    assert list(unified_diff(log_before, log_after)) == []
