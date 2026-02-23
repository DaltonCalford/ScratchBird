#coding:utf-8

"""
ID:          issue-5765
ISSUE:       5765
TITLE:       Creating SRP SYSDBA with explicit admin (-admin yes in gsec or grant admin role in create user) creates two SYSDBA accounts
DESCRIPTION:
  Test script should display only ONE record.
JIRA:        CORE-5496
FBTEST:      bugs.core_5496
NOTES:
    [08.12.2021] [pcisar]
    On Linux it fails with:
    Statement failed, SQLSTATE = 28000
    no permission for remote access to database security.db
  
    [18.09.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has alias defvined by variable REQUIRED_ALIAS.
       Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)

    Confirmed problem on 3.0.1.32609
    Checked on 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.730 (SS/CS) - both Linux and Windows.
"""
import locale
import re
import sys
from pathlib import Path
import time

import pytest
from firebird.qa import *

# Pre-defined alias for self-security DB in the QA_root/files/qa-databases.conf.
# This file must be copied manually to each testing FB home folder, with replacing
# databases.conf there:
#
REQUIRED_ALIAS = 'tmp_core_5496_alias'

db = db_factory()
db_clone = db_factory(filename = '#' + REQUIRED_ALIAS, do_not_create = True, do_not_drop = True)

act = python_act('db')

expected_stdout = """
    SEC$USER_NAME                   SYSDBA
    SEC$PLUGIN                      Srp
    Records affected: 1
"""
#----------------------------------------------------
def get_filename_by_alias(act: Action, alias_from_dbconf):
    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + alias_from_dbconf + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
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

    #------------------------------------------------------------------
    # Full path + filename of database to which we will try to connect:
    #
    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )

    return tmp_fdb

#----------------------------------------------------

@pytest.mark.version('>=3.0.1')
def test_1(act: Action, capsys):

    db_work_file = get_filename_by_alias(act, REQUIRED_ALIAS)
    dba_pswd = 'alterkey'

    sql_test = f"""
        set list on;
        create database '{REQUIRED_ALIAS}' user {act.db.user};
        create user {act.db.user} password '{dba_pswd}' using plugin Srp;
        create or alter user foo password '123' grant admin role using plugin Srp;
        create or alter user rio password '123' grant admin role using plugin Srp;
        create or alter user bar password '123' grant admin role using plugin Srp;
        commit;
        grant rdb$admin to {act.db.user} granted by foo;
        grant rdb$admin to {act.db.user} granted by rio;
        grant rdb$admin to {act.db.user} granted by bar;
        commit;
        set list on;
        set count on;
        -- This must return only 1 record:
        select sec$user_name, sec$plugin from sec$users where upper(sec$user_name) = upper('{act.db.user}') and upper(sec$plugin) = upper('srp');
        commit;
    """

    try:
        act.expected_stdout = f"""
            SEC$USER_NAME                   SYSDBA
            SEC$PLUGIN                      Srp
            Records affected: 1
        """
        act.isql(switches=['-q'], input=sql_test, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

    finally:
        for p in (db_work_file,):
            if Path.exists(p):
                # Change DB state to full shutdown in order to have ability to drop database file.
                # This is needed because when DB is self-security then it will be kept opened for 10s
                # (as it always occurs for default security<N>.fdb). Set linger to 0 does not help.
                # Attempt to use 'drop database' fails with:
                # "SQLSTATE = 40001 / lock time-out on wait transaction / -object ... is in use"
                act.gfix(switches=['-shut', 'full', '-force', '0', f'localhost:{p}', '-user', act.db.user, '-pas', dba_pswd], io_enc = locale.getpreferredencoding(), credentials = False, combine_output = True)
                p.unlink()
                assert '' == capsys.readouterr().out
