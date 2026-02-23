#coding:utf-8

"""
ID:          issue-7545
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7545
TITLE:       Server crash on some LockMemSize values
DESCRIPTION:
NOTES:
    [14.04.2023] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has alias defined by variable REQUIRED_ALIAS.
       Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)

    Confirmed crash on 5.0.0.1010, 4.0.3.2923, 3.0.11.33674
    Checked on 5.0.0.1014, 4.0.3.2929
"""

import os
import re
import locale
import subprocess
from pathlib import Path
import time

import pytest
from firebird.qa import *

# Name of alias for self-security DB in the QA_root/files/qa-databases.conf.
# This file must be copied manually to each testing FB homw folder, with replacing
# databases.conf there:
#
REQUIRED_ALIAS = 'tmp_gh_7545_alias'

db = db_factory(do_not_create = True, do_not_drop = True)
act = python_act('db' , substitutions = [ ('[ \t]+', ' ') ])

@pytest.mark.version('>=3.0.11')
def test_1(act: Action, capsys):
    
    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_7545_alias = $(dir_sampleDb)/qa/tmp_gh_7545.fdb
                # - then we extract filename: 'tmp_gh_7545.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    # Full path + filename of database to which we will try to connect:
    #
    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )

    sql_txt = f"""
        set bail on;
        set list on;
        create database 'localhost:{tmp_fdb}';
        select count(*) from mon$database;
        commit;
    """

    expected_stdout_isql = """
        COUNT 1
    """

    try:
        act.expected_stdout = expected_stdout_isql
        act.isql(switches = ['-q'], input = sql_txt, connect_db=False, combine_output = True, io_enc = locale.getpreferredencoding())

        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
        
    finally:
        tmp_fdb.unlink()
    
    assert '' == capsys.readouterr().out
