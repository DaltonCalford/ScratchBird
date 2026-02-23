#coding:utf-8

"""
ID:          issue-4720
ISSUE:       4720
TITLE:       Provide ability to specify extra-long name of log when doing gbak to avoid
  "attempt to store 256 bytes in a clumplet" message
DESCRIPTION:
JIRA:        CORE-4398
FBTEST:      bugs.core_4398
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    Starting backup...
    Backup finished.
    Delete backup file...
    Backup file deleted.
    Delete log file...
    Log file deleted.
"""

backup_file = temp_file('backup.fbk')
log_file = temp_file('A012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890.log')

@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys, log_file: Path, backup_file: Path):
    print ('Starting backup...')
    act.gbak(switches=['-b', '-v', '-y', str(log_file), str(act.db.db_path), str(backup_file)])
    print ('Backup finished.')
    if backup_file.is_file():
        print ('Delete backup file...')
        backup_file.unlink()
        print ('Backup file deleted.')
        print ('Delete log file...')
        log_file.unlink()
        print ('Log file deleted.')
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
