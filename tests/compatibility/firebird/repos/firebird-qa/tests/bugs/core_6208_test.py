#coding:utf-8

"""
ID:          issue-6453
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6453
TITLE:       CREATE DATABASE grant is lost in security.db after backup/restore cycle
DESCRIPTION:
    Ticket shows scenario with local protocol which allows security.db to be overwritten.
    This can not be done when we work using remote protocol, but we can exploit ability
    to change security DB. This is done by specifying parameter SecurityDatabase in databases.conf
    and its value is equal to alias of test database that we use:
        tmp_core_6208_alias = $(dir_sampleDb)/qa/tmp_core_6208.fdb
        {
            SecurityDatabase = tmp_core_6208_alias
        }

    We create this database using local protocol and specifying name of user that DOES NOT exist
    in 'common' security.db, see variable 'ALTER_DB_USER'. This user becomes OWNER of that DB.
    After this, we make backup and restore of this DB (using local protocol!), and finally we
    check that 'SHOW GRANTS' output contain grant to CREATE DATABASE for user <ALTER_DB_USER>.
    As final, temporary file with this DB must be manually removed because this is not act.db fixture.

JIRA:        CORE-6208
FBTEST:      bugs.core_6208
NOTES:
    [26.08.2020] pzotov
      IT CRUSIAL FOR THIS TEST DO MAKE ALL RESTORE AND FURTHER ACTIONS IN LOCAL/EMBEDDED PROTOCOL.
      Discissed with Alex, see letter 24.08.2020 19:49.

      Main problem is in SuperClassic: after restore finish, we can not connect to this DB by TCP,
      error is
        "Statement failed, SQLSTATE = 08006 / Error occurred during login, please check server firebird.log for details"
      Server log contains in this case: "Srp Server / connection shutdown / Database is shutdown."

      Checked initially on 4.0.0.1712 SC: 11s, 4.0.0.1714 SS, CS (7s, 16s).
      Checked again 26.08.2020 on 4.0.0.2173 SS/CS/SC.

    [08.08.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"

    Confirmed again problem: lost of grant on 4.0.0.1691.
    Checked on 5.0.0.591, 4.0.1.2692, 3.0.8.33535 - both on Windows and Linux.
"""
import os
import re
import locale
import subprocess
from pathlib import Path
import time

import pytest
from firebird.qa import *

for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

# Name of alias for self-security DB in the QA_root/files/qa-databases.conf.
# This file must be copied manually to each testing FB homw folder, with replacing
# databases.conf there:
#
REQUIRED_ALIAS = 'tmp_core_6208_alias'

db= db_factory()

act = python_act('db', substitutions=[('\t+', ' ')])

fbk_file = temp_file('tmp_core_6208.fbk')
res_file = temp_file('tmp_core_6208.restored.fdb')

expected_stdout = """
    mon$database.mon$owner          SYSDBA
    mon$database.mon$sec_database   Self
    rdb$database.rdb$linger         0
    WHOAMI                          TMP6208DBA
    sec$users.sec_user              TMP6208DBA
    sec$db_creators.sec$user_type   8
"""


@pytest.mark.version('>=3.0.6')
def test_1(act: Action, fbk_file: Path, res_file: Path, capsys):
    
    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_6147_alias = $(dir_sampleDb)/qa/tmp_core_6147.fdb
                # - then we extract filename: 'tmp_core_6147.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    # Full path + filename of database to which we will try to connect:
    #
    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )

    ALTER_DB_USER = 'tmp6208dba'

    init_sql = f'''
       set bail on;
       set list on;
       create database '{REQUIRED_ALIAS}' user {ALTER_DB_USER};
       create user {ALTER_DB_USER} password '123' using plugin Srp;
       grant create database to user tmp6208dba;
       alter database set linger to 0;
       exit;
    '''

    act.expected_stdout = ''
    try:
        act.isql(switches = ['-q'], input = init_sql, connect_db=False, credentials = False, combine_output = True)
        assert act.clean_stdout == ''
        act.reset()

        act.gbak(switches=['-b', '-user', act.db.user, REQUIRED_ALIAS, str(fbk_file)])
        assert act.clean_stdout == ''
        act.reset()

        act.gbak(switches=['-rep', '-user', ALTER_DB_USER, str(fbk_file), REQUIRED_ALIAS ])
        assert act.clean_stdout == ''
        act.reset()

        # NB: no need to drop user <ALTER_DB_USER> because this DB is self-security
        # thus all its users will disappear together with this DB:
        act.isql(switches = ['-q'], input = f"connect '{REQUIRED_ALIAS}' user {ALTER_DB_USER};show grants;", connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
        ptn_expected = re.compile( f'grant create database to user {ALTER_DB_USER}', re.IGNORECASE)
        for line in act.stdout.split('\n'):
            if ptn_expected.search(line):
                print(line)

        act.expected_stdout = f'GRANT CREATE DATABASE TO USER {ALTER_DB_USER.upper()}'
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
    finally:
        tmp_fdb.unlink()
