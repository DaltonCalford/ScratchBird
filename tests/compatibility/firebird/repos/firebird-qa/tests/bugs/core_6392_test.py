#coding:utf-8

"""
ID:          issue-6630
ISSUE:       6630
TITLE:       Space in database path prevent working gbak -se ... -b "pat to/database" backup
DESCRIPTION:
JIRA:        CORE-6392
FBTEST:      bugs.core_6392
NOTES:
    [24.07.2022] pzotov
    Reproduced problem on 4.0.0.2173.
        gbak: ERROR:multiple sources or destinations specified
        gbak: ERROR:    Exiting before completion due to errors
        gbak:Exiting before completion due to errors
    ::: NOTE :::
    Problem exists when DB file or folder has trailing character = '.' or ' ' (dot or space).
    Test does not use such case.
    Checked on 3.0.8.33535, 4.0.1.2692, 5.0.0.591
"""

import shutil
import re
import subprocess
from pathlib import Path
import pytest
from firebird.qa import *
import time

db = db_factory()

bkp_log = temp_file('backup_log.tmp')
res_log = temp_file('restore_log.tmp')

act = python_act('db', substitutions=[('[\t ]+', ' ')])

expected_backup_out = 'Backup completed OK.'
expected_restore_out = 'Restore completed OK.'

@pytest.mark.version('>=3.0.7')
@pytest.mark.platform('Windows')
# gbak: ERROR:cannot open status and error output file <function temp_file.<locals>.temp_file_fixture at 0x0000024389962040>
# gbak:Exiting before completion due to errors
def test_1(act: Action, bkp_log: Path, res_log: Path, capsys):
    p_base = Path(act.db.db_path).parent
    p_work = p_base / "..str@nge (path; folder & name),"
    Path(p_work).mkdir(parents=True, exist_ok=False)

    shutil.copy2(act.db.db_path, str(p_work))
    tmp_fdb = Path(p_work, Path(act.db.db_path).name)
    tmp_fbk = Path(p_work, Path(act.db.db_path).stem + ". fbk")

    error_pattern = re.compile(r'gbak:\s*ERROR(:)?', re.IGNORECASE)
    successful_backup_pattern = re.compile(r'gbak:closing file, committing, and finishing. \d+ bytes written', re.IGNORECASE)
    successful_restore_pattern = re.compile( r'gbak:finishing, closing, and going home', re.IGNORECASE )

    #-------------------------------------------------------
    # Backup using FB services API:
    #
    act.svcmgr(switches=[ 'action_backup', 'dbname', str(tmp_fdb), 'bkp_file', str(tmp_fbk), 'verbint', '999999'])
    svc_log = act.stdout
    svc_err = act.stderr

    for line in svc_err.split('\n'):
        print(line)

    for line in svc_log.split('\n'):
        if successful_backup_pattern.search(line):
            print(expected_backup_out)
            break

    act.expected_stdout = expected_backup_out
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #-------------------------------------------------------
    # Restore using FB services API:
    #
    act.svcmgr(switches=[ 'action_restore', 'bkp_file', str(tmp_fbk), 'dbname', str(tmp_fdb),  'verbint', '999999', 'res_replace'])
    svc_log = act.stdout
    svc_err = act.stderr

    for line in svc_err.split('\n'):
        print(line)

    for line in svc_log.split('\n'):
        if successful_restore_pattern.search(line):
            print(expected_restore_out)
            break

    act.expected_stdout = expected_restore_out
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #-------------------------------------------------------
    # Backup via 'gbak -se' (this failed on 4.0.0.2173):
    #
    act.gbak(switches=['-b', '-se', 'localhost:service_mgr', '-verbint', '99999', '-y', str(bkp_log), str(tmp_fdb), str(tmp_fbk)])
    with open(bkp_log, 'r') as f:
        for line in f:
            if error_pattern.search(line):
                print('Backup FAILED:', line)
                break
            if successful_backup_pattern.search(line):
                print(expected_backup_out)
                break
    act.expected_stdout = expected_backup_out
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #-------------------------------------------------------
    # Restore via 'gbak -se':
    #
    act.gbak(switches=['-rep', '-se', 'localhost:service_mgr', '-verbint', '99999', '-y', str(res_log), str(tmp_fbk), str(tmp_fdb) ])
    with open(res_log, 'r') as f:
        for line in f:
            if error_pattern.search(line):
                print('Restore FAILED:', line)
                break
            if successful_restore_pattern.search(line):
                print(expected_restore_out)
                break
    act.expected_stdout = expected_restore_out
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
