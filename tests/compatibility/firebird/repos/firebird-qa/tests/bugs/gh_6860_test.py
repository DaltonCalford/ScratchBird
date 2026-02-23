#coding:utf-8

"""
ID:          issue-6860
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6860
TITLE:       Create user statement fails with SQLSTATE = HY000 when using DataTypeCompatibility
DESCRIPTION:
    Test creates two self-security databases, each of them allows to create new SYSDBA.
    Alias for first of these DB contains 'DataTypeCompatibility = 2.5', second - 'DataTypeCompatibility = 3.0'
    (NB: parameter 'DataTypeCompatibility' is defined as per-database).
    Creation of SYSDBA in each of these databases must no raise any error.
FBTEST:      bugs.gh_6860
NOTES:
    [18.08.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has TWO aliases ('tmp_gh_6860_fb25_alias', 'tmp_gh_6860_fb30_alias')
       for each DataTypeCompatibility value (2.5 and 3.0).
       Both these aliases must defined SecurityDatabase which points to that alias, thus making such DB be self-security.
       Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Each value of req_alias must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)

    Confirmed again problem: 5.0.0.82, 4.0.1.2519 (got SQLSTATE = HY000 / add record error / -Incompatible data type).
    Checked on 5.0.0.623, 4.0.1.2692 - both on Windows and Linux.
"""

import re
import locale
from pathlib import Path

import pytest
from firebird.qa import *

# Name of alias for self-security DB in the QA_root/files/qa-databases.conf.
# This file must be copied manually to each testing FB homw folder, with replacing
# databases.conf there:
#
db = db_factory()

act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):

    
    for req_alias in ('tmp_gh_6860_fb25_alias', 'tmp_gh_6860_fb30_alias'):

        # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
        # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
        # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
        p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + req_alias + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
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

        tmp_dba_pswd = 'p@$$w6860'


        init_sql = f'''
           set bail on;
           set list on;

           create database '{req_alias}' user {act.db.user};

           select '{req_alias}' as req_alias, 'DB creation completed OK.' as msg from rdb$database;
           alter database set linger to 0;
           create user {act.db.user} password '{tmp_dba_pswd}' using plugin Srp;
           commit;
        '''

        try:
            act.expected_stdout = f"""
                REQ_ALIAS                       {req_alias}
                MSG                             DB creation completed OK.
            """

            act.isql(switches = ['-q'], input = init_sql, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()

            # Change DB state to full shutdown in order to have ability to drop database file.
            # This is needed because when DB is self-security then it will be kept opened for 10s
            # (as it always occurs for common security.db). Set linger to 0 does not help.
            act.gfix(switches=['-shut', 'full', '-force', '0', f'localhost:{req_alias}', '-user', act.db.user, '-pas', tmp_dba_pswd], io_enc = locale.getpreferredencoding(), credentials = False, combine_output = True)
            act.stdout = capsys.readouterr().out
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()
        finally:
            tmp_fdb.unlink()
