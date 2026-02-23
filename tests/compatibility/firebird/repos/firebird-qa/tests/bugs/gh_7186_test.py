#coding:utf-8

"""
ID:          issue-7186
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7186
TITLE:       Nbackup RDB$BACKUP_HISTORY cleanup
DESCRIPTION:
    Test verifies ability to specify command switches '-clean_history' and 'keep N rows' in NBACKUP utility.
    Temporary user and role are created. System privilege USE_NBACKUP_UTILITY is granted to role, and role is granted to this user.
    All executions of NBACKUP are performed as this temporary user.

    Several levels of incremental backups are created WITHOUT this command switches (see 'NBK_COUNT_BEFORE_CLEANUP').
    Then we run nbackup <NBK_COUNT_WITH_CLEANUP> times with switch '-clean_hist' and require to keep <NBK_KEEP_ROWS> rows.
    Finally, we check content of RDB$BACKUP_HISTORY table: only NBK_KEEP_ROWS records must be selected from it.

    ::: NB :::
    It seems that currently firebird-driver does not support Services API parameters related to cleanup history
    (isc_spb_nbk_clean_history; isc_spb_nbk_keep_days <int>	and isc_spb_nbk_keep_rows <int>).
    Test will be modified after introducing this supoport.

    Checked on 5.0.0.967 SS/CS, 4.0.3.2904 SS/CS -- all fine.
"""

import pytest
from firebird.qa import *
from typing import List
from pathlib import Path
import subprocess
import re
import locale
import time


db = db_factory()

act = python_act('db')

NBK_COUNT_BEFORE_CLEANUP = 6
tmp_nbk_files_before_cleanup = temp_files( [ f'tmp_7186.nbk{i}' for i in range(NBK_COUNT_BEFORE_CLEANUP) ] )

NBK_COUNT_WITH_CLEANUP = 4
tmp_nbk_files_with_cleanup = temp_files( [ f'tmp_7186.nbk{i}' for i in range(NBK_COUNT_BEFORE_CLEANUP, NBK_COUNT_BEFORE_CLEANUP + NBK_COUNT_WITH_CLEANUP) ] )

NBK_KEEP_ROWS = 3

tmp_user = user_factory('db', name='gh_7186_user', password = '123')
tmp_role = role_factory('db', name='gh_7186_role')

expected_stdout = """
"""

@pytest.mark.version('>=4.0.3')
def test_1(act: Action, tmp_nbk_files_before_cleanup: List[Path], tmp_nbk_files_with_cleanup: List[Path], tmp_user: User, tmp_role: Role, capsys):
    init_sql = f"""
        recreate table test(id int generated always as identity (start with 0));
        grant insert,select on test to public;
        alter role {tmp_role.name} set system privileges to USE_NBACKUP_UTILITY;
        grant default {tmp_role.name} to user {tmp_user.name};
        commit;
    """
    act.expected_stdout = ''
    act.isql(switches=[], input = init_sql, combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    with act.db.connect() as con:
        for i,tmp_nbk_i in enumerate(tmp_nbk_files_before_cleanup):
            con.execute_immediate('insert into test default values')
            con.commit()
            act.expected_stderr = ''
            act.nbackup(switches=['-user', tmp_user.name, '-pas', tmp_user.password, '-b', str(i), act.db.dsn, tmp_nbk_i], credentials = False, io_enc = locale.getpreferredencoding())
            assert act.clean_stderr == act.clean_expected_stderr
            act.reset()


    # Now we try to cleanup history of nbackups, using only <N> ROWS:
    # act.nbackup(switches=['-user', tmp_user.name, '-pas', tmp_user.password, '-b', str(i), act.db.dsn, tmp_nbk_i], combine_output = True, credentials = False, io_enc = locale.getpreferredencoding())
    # nbackup -B 10 C:\temp\pytest-of-PashaZ\pytest-110\test_10\TEST.FDB C:\temp\pytest-of-PashaZ\pytest-110\test_10\tmp_7186.nbk10 -clean_hist -keep 3 row

    for i,tmp_nbk_i in enumerate(tmp_nbk_files_with_cleanup):
        act.expected_stderr = ''
        act.nbackup(switches=['-user', tmp_user.name, '-pas', tmp_user.password, '-b', str(NBK_COUNT_BEFORE_CLEANUP + i), act.db.dsn, tmp_nbk_i, '-clean_hist', '-keep', str(NBK_KEEP_ROWS), 'row'], credentials = False, io_enc = locale.getpreferredencoding())
        assert act.clean_stderr == act.clean_expected_stderr
        act.reset()
    
    test_sql = """
        set list on;
        set count on;
        select rdb$backup_id, rdb$backup_level
        from rdb$backup_history
        order by rdb$backup_id;
    """
    act.expected_stdout = f"""
        RDB$BACKUP_ID                   {NBK_COUNT_BEFORE_CLEANUP + NBK_COUNT_WITH_CLEANUP - 2}
        RDB$BACKUP_LEVEL                {NBK_COUNT_BEFORE_CLEANUP + NBK_COUNT_WITH_CLEANUP - 3}

        RDB$BACKUP_ID                   {NBK_COUNT_BEFORE_CLEANUP + NBK_COUNT_WITH_CLEANUP - 1}
        RDB$BACKUP_LEVEL                {NBK_COUNT_BEFORE_CLEANUP + NBK_COUNT_WITH_CLEANUP - 2}

        RDB$BACKUP_ID                   {NBK_COUNT_BEFORE_CLEANUP + NBK_COUNT_WITH_CLEANUP - 0}
        RDB$BACKUP_LEVEL                {NBK_COUNT_BEFORE_CLEANUP + NBK_COUNT_WITH_CLEANUP - 1}

        Records affected: {NBK_KEEP_ROWS}
    """
    act.isql(switches=[], input = test_sql, combine_output=True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
