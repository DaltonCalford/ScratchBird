#coding:utf-8

"""
ID:          None
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/fd0fa8a3a58fbfe7fdc0641b4e48258643d72127
TITLE:       Let include file name into error message when creation of temp file failed.
DESCRIPTION:
    Test uses pre-created databases.conf which has alias 'tmp_fd0fa8a3_alias' (see variable REQUIRED_ALIAS).
    Database file for that alias must NOT exist in the $(dir_sampleDb)/qa/ subdirectory: it will be created here.
    For this alias parameter TempTableDirectory is defined and it points to invalid/inaccessible directory.
    Currently its value is: '<>' (without single quotes), so there is no way to create any file in it.
    We check that:
        * client still has ability to create GTT and put data in it, without getting error;
        * firebird.log will have appropriate message about problem with creating file ('fb_*****') in TempTableDirectory
NOTES:
    [12.08.2024] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    4. Invalid value of TempTableDirectory causes appropriate message to appear in firebird.log TWO times: first when QA-plugin prepares
       test database (and this is out of scope to be checked by this test), and second when we try to insert data in the GTT.
       DDL statement ('create global temporary table') does NOT cause any message in firebird.log until we do not try to add data in it.
       Because of that, difference between content of firebird.log will contain only ONE message "Error creating file...".

    Parameter 'TempTableDirectory' exists in FB-4.x since 20.04.2021, commit f2805020a6f34d253c93b8edac6068c1b35f9b89., build 4.0.0.2436.
    Checked on Windows/Linux, 6.0.0.423, 5.0.2.1477, 4.0.6.3141.
"""

import os
import re
import locale
from difflib import unified_diff
from pathlib import Path

import pytest
from firebird.qa import *

REQUIRED_ALIAS = 'tmp_fd0fa8a3_alias'

substitutions = [ ('[ \t]+', ' ')
                 ,('Error creating file in TempTableDirectory.*', 'Error creating file in TempTableDirectory')
                 ,('I/O error during "((CreateFile\\s+\\(create\\))|open)" operation for file.*', 'I/O error during CreateFile operation for file')
                ]
db = db_factory(filename = '#' + REQUIRED_ALIAS)
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=4.0')
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
                #     tmp_NNNN_alias = $(dir_sampleDb)/qa/tmp_qa_NNNN.fdb 
                # - then we extract filename: 'tmp_qa_NNNN.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    check_sql = f"""
        set bail on;
        recreate global temporary table test(x int);
        set count on;
        insert into test(x) values(1);
    """

    # Check-1: no error must be issued on client-side, all records have to be inserted:
    #
    expected_stdout = f"""
        Records affected: 1
    """

    # Get content of firebird.log BEFORE test.
    # ::: NB :::
    # At this point firebird.log must already contain message about unable to create file because of inaccessible TempTableDirectory value.
    # This message was added when test database have been created by QA-plugin, i.e. out of this test code. So, the difference between
    # log content will NOT contain this message!
    #
    log_before = act.get_firebird_log()

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = check_sql, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # Get content of firebird.log AFTER test.
    # ONLY ONE message about invalid TempTableDirectory value will be taken in account!
    #
    log_after = act.get_firebird_log()

    #----------------------------------------------------

    # Check-2: firebird.log must have message (partially localized):
    # on Windows:
    #     Database: ...
    #     Error creating file in TempTableDirectory "..."
    #     I/O error during "CreateFile (create)" operation for file "..."
    #     Error while trying to create file
    #     Syntax error in file name, folder name, or volume label [ NB: THIS LINE CAN BE LOCALIZED ]
    #
    # on LINUX:
    #     Database: ...
    #     Error creating file in TempTableDirectory "..."
    #     I/O error during "open" operation for file "..."
    #     Error while trying to create file
    #     No such file or directory

    allowed_patterns = [  re.compile('Error creating file in TempTableDirectory',re.IGNORECASE),
                          re.compile('I/O error during "((CreateFile\\s+\\(create\\))|open)" operation for file',re.IGNORECASE),
                          re.compile('Error while trying to create file',re.IGNORECASE)
                       ]

    for line in unified_diff(log_before, log_after):
        if (msg := line.strip()):
            if msg.startswith('+') and act.match_any(msg, allowed_patterns):
                print(msg[1:])

    act.expected_stdout = """
        Error creating file in TempTableDirectory
        I/O error during "CreateFile (create)" operation for file
        Error while trying to create file
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
