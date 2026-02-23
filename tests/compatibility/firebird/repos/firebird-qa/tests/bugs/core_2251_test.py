#coding:utf-8

"""
ID:          issue-2677
ISSUE:       2677
TITLE:       gbak doesn't return error code
DESCRIPTION:
  Inaccessible folder is defined here as $tmp + GUID (i.e. it 100% not yet exists).
  We have to check allof kind for inaccessible file:
    * .fbk when trying to make backup of existing database;
    * .fdb when trying to restore from existing .fbk;
    * .log - for any of these operation
  We are NOT interested when all of these files are in accessible folder(s).

  Query to obtain set of interested combinations (all of them should have retcode = 1):
    with
    a as (
        select 'backup' as action from rdb$database union all
        select 'restore' from rdb$database
    )
    ,s as (
        select 'inaccessible' as path_to_source_file from rdb$database union all
        select 'accessible' from rdb$database
    )
    ,t as (
        select 'inaccessible' as path_to_target_file from rdb$database union all
        select 'accessible' from rdb$database
    )
    ,g as (
        select 'inaccessible' as path_to_action_log from rdb$database union all
        select 'accessible' from rdb$database
    )
    select * from a,s,t,g
    where NOT (s.path_to_source_file = 'accessible' and t.path_to_target_file = 'accessible' and g.path_to_action_log = 'accessible')
    order by 1,2,3,4;

  Confirmed wrong results on: 4.0.0.1714 SC; 4.0.0.1715 CS;  3.0.5.33221 SC; 3.0.5.33225 CS
JIRA:        CORE-2251
FBTEST:      bugs.core_2251
"""

import pytest
import subprocess
from uuid import uuid4
from pathlib import Path
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('\t+', ' ')])

expected_stdout = """
    backup  source_fdb=accessible          target_fbk=accessible          log_file=inaccessible:         1
    backup  source_fdb=accessible          target_fbk=inaccessible        log_file=accessible:           1
    backup  source_fdb=accessible          target_fbk=inaccessible        log_file=inaccessible:         1
    backup  source_fdb=inaccessible        target_fbk=accessible          log_file=accessible:           1
    backup  source_fdb=inaccessible        target_fbk=accessible          log_file=inaccessible:         1
    backup  source_fdb=inaccessible        target_fbk=inaccessible        log_file=accessible:           1
    backup  source_fdb=inaccessible        target_fbk=inaccessible        log_file=inaccessible:         1
    restore source_fbk=accessible          target_fdb=accessible          log_file=inaccessible:         1
    restore source_fbk=accessible          target_fdb=inaccessible        log_file=accessible:           1
    restore source_fbk=accessible          target_fdb=inaccessible        log_file=inaccessible:         1
    restore source_fbk=inaccessible        target_fdb=accessible          log_file=accessible:           1
    restore source_fbk=inaccessible        target_fdb=accessible          log_file=inaccessible:         1
    restore source_fbk=inaccessible        target_fdb=inaccessible        log_file=accessible:           1
    restore source_fbk=inaccessible        target_fdb=inaccessible        log_file=inaccessible:         1
"""

inaccessible_dir = temp_file(uuid4().hex)
correct_fbk = temp_file('tmp_2251.fbk')
correct_res = temp_file('tmp_2251.tmp')
correct_log = temp_file('tmp_2251.log')

@pytest.mark.version('>=3.0.5')
def test_1(act: Action, capsys, inaccessible_dir: Path, correct_fbk: Path,
           correct_res: Path, correct_log: Path):
    correct_fdb = act.db.db_path
    invalid_fdb = inaccessible_dir / 'tmp_2251.fdb'
    invalid_fbk = inaccessible_dir / 'tmp_2251.fbk'
    #invalid_res = inaccessible_dir / 'tmp_2251.tmp'
    invalid_log = inaccessible_dir / 'tmp_2251.log'
    #
    retcode = subprocess.call([act.vars['gbak'], '-b', '-se', 'localhost:service_mgr',
                               correct_fdb, correct_fbk, '-y', invalid_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('backup  source_fdb=accessible          target_fbk=accessible          log_file=inaccessible:'.ljust(100), retcode)

    retcode = subprocess.call([act.vars['gbak'], '-b', '-se', 'localhost:service_mgr',
                               correct_fdb, invalid_fbk, '-y', correct_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('backup  source_fdb=accessible          target_fbk=inaccessible        log_file=accessible:'.ljust(100), retcode)

    retcode = subprocess.call([act.vars['gbak'], '-b', '-se', 'localhost:service_mgr',
                               correct_fdb, invalid_fbk, '-y', invalid_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('backup  source_fdb=accessible          target_fbk=inaccessible        log_file=inaccessible:'.ljust(100), retcode)

    retcode = subprocess.call([act.vars['gbak'], '-b', '-se', 'localhost:service_mgr',
                               invalid_fdb, correct_fbk, '-y', correct_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('backup  source_fdb=inaccessible        target_fbk=accessible          log_file=accessible:'.ljust(100), retcode)

    retcode = subprocess.call([act.vars['gbak'], '-b', '-se', 'localhost:service_mgr',
                               invalid_fdb, correct_fbk, '-y', invalid_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('backup  source_fdb=inaccessible        target_fbk=accessible          log_file=inaccessible:'.ljust(100), retcode)

    retcode = subprocess.call([act.vars['gbak'], '-b', '-se', 'localhost:service_mgr',
                               invalid_fdb, invalid_fbk, '-y', correct_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('backup  source_fdb=inaccessible        target_fbk=inaccessible        log_file=accessible:'.ljust(100), retcode)

    retcode = subprocess.call([act.vars['gbak'], '-b', '-se', 'localhost:service_mgr',
                               invalid_fdb, invalid_fbk, '-y', invalid_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('backup  source_fdb=inaccessible        target_fbk=inaccessible        log_file=inaccessible:'.ljust(100), retcode)

    ######################################################################################
    act.gbak(switches=['-b', '-se', 'localhost:service_mgr', correct_fdb, correct_fbk] )
    ######################################################################################

    retcode = subprocess.call([act.vars['gbak'], '-rep', '-se', 'localhost:service_mgr',
                               correct_fbk, correct_fdb, '-y', invalid_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('restore source_fbk=accessible          target_fdb=accessible          log_file=inaccessible:'.ljust(100), retcode)

    retcode = subprocess.call([act.vars['gbak'], '-rep', '-se', 'localhost:service_mgr',
                               correct_fbk, invalid_fdb, '-y', correct_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('restore source_fbk=accessible          target_fdb=inaccessible        log_file=accessible:'.ljust(100), retcode)

    retcode = subprocess.call([act.vars['gbak'], '-rep', '-se', 'localhost:service_mgr',
                               correct_fbk, invalid_fdb, '-y', invalid_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('restore source_fbk=accessible          target_fdb=inaccessible        log_file=inaccessible:'.ljust(100), retcode)

    retcode = subprocess.call([act.vars['gbak'], '-rep', '-se', 'localhost:service_mgr',
                               invalid_fbk, correct_fdb, '-y', correct_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('restore source_fbk=inaccessible        target_fdb=accessible          log_file=accessible:'.ljust(100), retcode)

    retcode = subprocess.call([act.vars['gbak'], '-rep', '-se', 'localhost:service_mgr',
                               invalid_fbk, correct_fdb, '-y', invalid_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('restore source_fbk=inaccessible        target_fdb=accessible          log_file=inaccessible:'.ljust(100), retcode)

    retcode = subprocess.call([act.vars['gbak'], '-rep', '-se', 'localhost:service_mgr',
                               invalid_fbk, invalid_fdb, '-y', correct_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print( 'restore source_fbk=inaccessible        target_fdb=inaccessible        log_file=accessible:'.ljust(100), retcode )

    retcode = subprocess.call([act.vars['gbak'], '-rep', '-se', 'localhost:service_mgr',
                               invalid_fbk, invalid_fdb, '-y', invalid_log ],
                              stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    print('restore source_fbk=inaccessible        target_fdb=inaccessible        log_file=inaccessible:'.ljust(100), retcode)
    #
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

