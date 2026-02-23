#coding:utf-8

"""
ID:          issue-6560
ISSUE:       6560
TITLE:       NBACKUP locks db file on error
DESCRIPTION:
    We create level-0 copy of test DB (so called "stand-by DB") and obtain DB backup GUID for just performed action.
    Then we create incremental copy using this GUID ("nbk_level_1") and obtain new DB backup GUID again.
    After this we repeat - create next incrementa copy using this (new) GUID ("nbk_level_2").

    (note: cursor for 'select h.rdb$guid from rdb$backup_history h order by h.rdb$backup_id desc rows 1' can be used
    to get last database backup GUID instead of running 'gstat -h').

    Further, we try to apply two incremental copies but in WRONG order of restore: specify <nbk_level_2> twice instead
    of proper order: <nbk_level_1> and after this - <nbk_level_2>.

    First and second attempts should issue THE SAME message:
    "Wrong order of backup files or invalid incremental backup file detected, file: <nbk_02>".

    We do this check both for NBACKUP and FBSVCMGR.

    Confirmed bug on 4.0.0.2000: second attempt to run restore using FBSVCMGR issues:
    =====
        Error opening database file: [disk]:\\path	o\\standby_db.dfb
        process cannot access the file <nbk_level_2> because it is being used by another process
    =====
    - and file <nbk_level_2> could not be deleted after this until restart of FB.
    
    See also: 
    https://github.com/FirebirdSQL/firebird/issues/6728 (ex. CORE-6498)

JIRA:        CORE-6319
FBTEST:      bugs.core_6319
"""

import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    Attempt 1. Error message is expected.
    Attempt 2. Error message is expected.
    Attempt 1. Error message is expected.
    Attempt 2. Error message is expected.
"""

nbk_level_0 = temp_file('core_6319_standby.fdb')
nbk_level_1 = temp_file('core_6319.nbk_01')
nbk_level_2 = temp_file('core_6319.nbk_02')
db_3x_restore = temp_file('core_6319_restored_in_3x.nbk_02')

@pytest.mark.version('>=3.0.6')
def test_1(act: Action, db_3x_restore: Path, nbk_level_0: Path, nbk_level_1: Path,
           nbk_level_2: Path, capsys):

    def cleanup(files):
        for item in files:
            if item.is_file():
                item.unlink()

    def check_stderr(step: int, source: str):
        for line in act.stderr.splitlines():
            if line:
                if act.is_version('<4') and 'Invalid level 2 of incremental backup file' in line:
                    print(f'Attempt {step}. Error message is expected.')
                elif act.is_version('>=4') and ('Wrong order of backup' in line or 'invalid incremental backup' in line):
                    print(f'Attempt {step}. Error message is expected.')
                else:
                    print(f'Attempt {step}. UNEXPECTED {source} message: {line}')

    get_last_bkup_guid_sttm = 'select h.rdb$guid from rdb$backup_history h order by h.rdb$backup_id desc rows 1'
    # 1. Create standby copy: make clone of source DB using nbackup -b 0:
    act.nbackup(switches=['-b', '0', str(act.db.db_path), str(nbk_level_0)])
    #nbk_level_0.chmod(16895) # Set permissions
    # Read DB-backup GUID after this 1st nbackup run:
    with act.db.connect() as con:
        c = con.cursor()
        db_guid = c.execute(get_last_bkup_guid_sttm).fetchone()[0]
    # Create 1st copy using just obtained DB backup GUID:
    act.reset()
    act.nbackup(switches=['-b', db_guid if act.is_version('>=4') else '1',
                            str(act.db.db_path), str(nbk_level_1)])
    # Re-read DB backup GUID: it is changed after each new nbackup!
    with act.db.connect() as con:
        c = con.cursor()
        db_guid = c.execute(get_last_bkup_guid_sttm).fetchone()[0]
    # Create 2nd copy using LAST obtained GUID of backup:
    act.reset()
    act.nbackup(switches=['-b', db_guid if act.is_version('>=4') else '2',
                            str(act.db.db_path), str(nbk_level_2)])
    # Set permissions
    nbk_level_0.chmod(16895)
    nbk_level_1.chmod(16895)
    nbk_level_2.chmod(16895)
    # Try to merge standby DB and SECOND copy, i.e. wrongly skip 1st incremental copy.
    # NB: we do this TWICE. And both time this attempt should fail with:
    # "Wrong order of backup files or invalid incremental backup file detected, file:  ..."
    if act.is_version('>=4'):
        nbk_wrong_call = ['-inplace', '-restore', act.get_dsn(nbk_level_0), str(nbk_level_2)]
    else:
        nbk_wrong_call = ['-restore', str(db_3x_restore), str(nbk_level_0), str(nbk_level_2)]
    for step in [1, 2]:
        act.reset()
        act.expected_stderr = "We expect errors"
        act.nbackup(switches=nbk_wrong_call)
        check_stderr(step, 'nbakup')
        cleanup([db_3x_restore])
    # Try to do the same using FBSVCMGR.
    # We also do this twice and both attempts must finish the same as previous pair:
    # Wrong order of backup files or invalid incremental backup file detected, file: C:/FBTESTING/qa/fbt-repo/tmp/tmp_core_6319.nbk_02
    if act.is_version('>=4'):
        fbsvc_call_01 = ['action_nrest', 'nbk_inplace', 'dbname', act.get_dsn(nbk_level_0),
                         'nbk_file', str(nbk_level_1)]
        fbsvc_call_02 = ['action_nrest', 'nbk_inplace', 'dbname', act.get_dsn(nbk_level_0),
                            'nbk_file', str(nbk_level_2)]
    else:
        fbsvc_call_01 = ['action_nrest', 'dbname', str(db_3x_restore), 'nbk_file', str(nbk_level_0),
                         'nbk_file', str(nbk_level_1), 'nbk_file', str(nbk_level_2)]
        fbsvc_call_02 = ['action_nrest', 'dbname', str(db_3x_restore), 'nbk_file', str(nbk_level_0),
                            'nbk_file', str(nbk_level_2), 'nbk_file', str(nbk_level_2)]
    for step in [1, 2]:
        act.reset()
        act.expected_stderr = "We expect errors"
        act.svcmgr(switches=fbsvc_call_02)
        check_stderr(step, 'svcmgr')
        cleanup([db_3x_restore])
    # Try to apply incremental copies in proper order, also using FBSVCMGR.
    # No errors must occur in this case:
    act.reset()
    act.svcmgr(switches=fbsvc_call_01)
    cleanup([db_3x_restore])
    if act.is_version('>=4'):
        act.reset()
        act.svcmgr(switches=fbsvc_call_02)
    # Check
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
