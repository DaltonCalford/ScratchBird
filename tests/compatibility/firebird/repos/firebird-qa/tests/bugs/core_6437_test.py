#coding:utf-8

"""
ID:          issue-6672
ISSUE:       6672
TITLE:       gfix cannot set big value for buffers
DESCRIPTION:
  New database must be created in THIS code rather than "outside" (in fbtest) for reproducing bug.
  Confirmed bug on 4.0.0.2225.

  ::: NB :::
  On build 4.0.0.2225 attempt to change buffers via gfix failed when value was >= 524337 for DB with page_size=8192.
  Perhaps this was related to allocating memory more than 2Gb plus small addition (exact value: 2.00018692 Gb).
  Ouput of gfix in this case contained:
  =======
    I/O error during "ReadFile" operation for file "C:\\FBTESTING\\QA\\FBT-REPO\\TMP\\TMP_GFIX_6437.FDB"
    -Error while trying to read from file
    -Invalid attempt to access memory address << localized message!
  =======
  Test creates database with page size = 8192 and tries to run 'gfix -buffers 524337', with further checking that:
  * output of gfix is empty;
  * page buffers has been changed and equals to this new value.

  DO NOT change this value to some "really too big" value otherwise it will fail with "unable to allocate memory"!
JIRA:        CORE-6437
FBTEST:      bugs.core_6437
NOTES:
    [08.10.2022] pzotov
        4.0.1.2692
        Fails on CentOS-7.9.2009 "with unable to allocate memory from operating system"
        Decided to restrict execution on Windows only.
"""
import platform
import pytest
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('^((?!(File|file|Page buffers)|(Page size)).)*$', ''),
                                      ('[\t ]+', ' ')])

expected_stdout = """
    Page size 8192
    Page buffers 524337
"""

test_db = temp_file('tmp_gfix_6437.fdb')

@pytest.mark.skipif(platform.system() != 'Windows', reason='FIXME: see notes')
@pytest.mark.version('>=3.0.8')
def test_1(act: Action, test_db: Path):
    act.isql(switches=['-q'], connect_db=False,
               input=f"create database '{test_db}' page_size 8192;")
    #
    act.reset()
    act.gfix(switches=[str(test_db), '-buffers', '524337'])
    act.reset()
    #
    act.expected_stdout = expected_stdout
    act.gstat(switches=['-h', str(test_db)], connect_db=False)
    assert act.clean_stdout == act.clean_expected_stdout
