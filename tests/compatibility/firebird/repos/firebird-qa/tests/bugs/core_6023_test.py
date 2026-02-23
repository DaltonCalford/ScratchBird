#coding:utf-8

"""
ID:          issue-6273
ISSUE:       6273
TITLE:       FB4 unable to overwrite older ods database
DESCRIPTION:
  Database (employee.fdb) with ODS 11.2 has been compressed to .zip and stored in {FBT_REPO}/files subdirectory.
  Test unpacks this .fdb and:
    1) tries to print its header (using gstat -h) - it must fail with "Wrong ODS version, expected NN, encountered 11"
       (where <NN> is ODS major number that is supported by current FB version);
    2) makes attempt to replace this .fdb by following action:
        <current_FB>/gbak -b <dsn> stdout | <current_FB>/gbak -rep stdin <fdb_with_ODS_11_2>
    3) tries to make connection to just restored DB and write result.

  If replacement was successfull then connection *must* be estabished and it is enough to print SIGN(current_connection).
  Outcome of this ("1") means that all completed OK.

  Confirmed bug on 4.0.0.1803: attempt to restore fails with:
    gbak: ERROR:unsupported on-disk structure for file ...; found 11.2, support 13.0
    gbak: ERROR:    IProvider::attachDatabase failed when loading mapping cache
    gbak: ERROR:failed to create database localhost:...
    gbak:Exiting before completion due to errors

  ::: CAUTION :::
  DO NOT try to run this test on any other FB build just after check build 4.0.0.1803!
  One need to STOP instance 4.0.0.1803 before this or wait for at least 130 seconds,
  otherwise checked FB will crash. Problem relates to shmem-files in C:\\ProgramData\\Firebird\\.
  This problem has the same nature that is described in CORE-6476.
  See letter to Vlad et al 16.02.2021 09:02
  ("Crash of 4.0.0.2365 when attempt to get server version just after doing the same on 4.0.0.1803")
JIRA:        CORE-6023
FBTEST:      bugs.core_6023
"""

import pytest
import zipfile
from pathlib import Path
from firebird.qa import *

# Database is extracted from zip
db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' '), ('expected [\\d]+', 'expected NN')])

expected_stdout = """
    RESTORE_WITH_REPLACE_RESULT     1
"""

expected_stderr = """
    Wrong ODS version, expected 13, encountered 11
"""

fdb_112_file = temp_file('core_6023-ods-11_2.fdb')
fbk_file = temp_file('core_6023.fbk')

@pytest.mark.version('>=4.0')
def test_1(act: Action, fdb_112_file: Path, fbk_file: Path):
    zipped_fdb_file = zipfile.Path(act.files_dir / 'core_6023-ods-11_2-fdb.zip',
                                   at='core_6023-ods-11_2.fdb')
    fdb_112_file.write_bytes(zipped_fdb_file.read_bytes())
    # Change permissions
    fdb_112_file.chmod(16895)
    # Ensure that we really have deal with .fdb file with old ODS.
    act.expected_stderr = expected_stderr
    act.gstat(switches=['-h', str(fdb_112_file)], connect_db=False)
    assert act.clean_stderr == act.clean_expected_stderr
    # Backup work database and restore over extracted db
    act.reset()
    act.gbak(switches=['-b', act.db.dsn, str(fbk_file)])
    act.reset()
    act.gbak(switches=['-rep', str(fbk_file), act.get_dsn(fdb_112_file)])
    #
    act.reset()
    act.expected_stdout = expected_stdout
    act.isql(switches=['-q', act.get_dsn(fdb_112_file)], connect_db=False,
             input='set list on; select sign(current_connection) as restore_with_replace_result from rdb$database;')
    assert act.clean_stdout == act.clean_expected_stdout
