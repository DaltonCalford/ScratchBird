#coding:utf-8

"""
ID:          issue-2436
ISSUE:       2436
TITLE:       TimeStamp in the every line output gbak.exe utility
DESCRIPTION:
  Database for this test was created beforehand and filled-up with all possible kind of objects:
  domain, table, view, standalone procedure & function, package, trigger, sequence, exception and role.
  Then backup was created for this DB and it was packed into .zip archive - see files/core_1999_nn.zip.
  This test extract .fbk from .zip and does its restore and then - again backup, but with option 'res_stat tdrw'.
  Both processes are logged. Finally, we parse these logs by counting lines which contain NO statistics.
  Presence of statistics is determined by analyzing corresponding tokens of each line. Token which contains only
  digits (with exception of "dot" and "comma" characters) is considered as VALUE related to some statistics.
  Backup log should contain only single (1st) line w/o statistics, restore - 1st and last lines.
JIRA:        CORE-1999
FBTEST:      bugs.core_1999
"""

import pytest
from firebird.qa import *
from firebird.driver import SrvBackupFlag, SrvRestoreFlag

db = db_factory()

act = python_act('db')

fbk_file = temp_file('pytest-run.fbk')

@pytest.mark.version('>=3.0.5')
def test_1(act: Action, fbk_file):
    src_backup = act.vars['backups'] / 'core1999_30.fbk'
    with act.connect_server() as srv:
        srv.database.restore(database=act.db.db_path, backup=src_backup,
                             flags=SrvRestoreFlag.REPLACE,
                             verbose=True, stats='TDWR')
        restore_log = srv.readlines()
        srv.database.backup(database=act.db.db_path, backup=fbk_file,
                            verbose=True, stats='TDWR')
        backup_log = srv.readlines()
        #
        # Backup log should contain SINGLE row without statistics, in its header (1st line):
        # gbak: time     delta  reads  writes
        #
        rows_without_stat = 0
        for line in backup_log:
            tokens = line.split()
            if not (tokens[1].replace('.', '', 1).replace(',', '', 1).isdigit() and
                    tokens[2].replace('.', '', 1).replace(',', '', 1).isdigit() and
                    tokens[3].replace('.', '', 1).replace(',', '', 1).isdigit() and
                    tokens[4].replace('.', '', 1).replace(',', '', 1).isdigit()):
                rows_without_stat = rows_without_stat + 1
        assert rows_without_stat == 1
        #
        # Restore log should contain TWO rows without statistics, first and last:
        # gbak: time     delta  reads  writes
        # gbak:adjusting the ONLINE and FORCED WRITES flags
        #
        rows_without_stat = 0
        for line in restore_log:
            tokens = line.split()
            if not (tokens[1].replace('.', '', 1).replace(',', '', 1).isdigit() and
                    tokens[2].replace('.', '', 1).replace(',', '', 1).isdigit() and
                    tokens[3].replace('.', '', 1).replace(',', '', 1).isdigit() and
                    tokens[4].replace('.', '', 1).replace(',', '', 1).isdigit()):
                rows_without_stat = rows_without_stat + 1
        assert rows_without_stat == 2

