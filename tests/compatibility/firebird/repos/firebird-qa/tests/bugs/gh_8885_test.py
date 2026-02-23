#coding:utf-8

"""
ID:          None
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8885
TITLE:       AV when lock manager settings is misconfigured
DESCRIPTION:
    Test uses pre-created databases.conf which has alias 'gh_8885_alias' (see variable REQUIRED_ALIAS).
    Database file for that alias must NOT exist in the $(dir_sampleDb)/qa/ subdirectory: it will be created here.
    For this alias parameters LockMemSize and LockHashSlots with values provided in the ticket.
    We check that:
        * connection to DB can be done, query to RDB$CONFIG returns proper values of these parameters.
        * firebird.log will have no messages related to errors
NOTES:
    [12.08.2024] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    4. AV raised when LockMemSize had minimal size and LockHashSlots greater than ~32K (letter from Vlad, mailbox pz@ibase.ru, 02.02.26 1319.
    5. Commits that fixed problem:
        6.x: https://github.com/FirebirdSQL/firebird/commit/b93f2781a8d8d0ae746b1ae3c47894c7debf52a0
        5.x: https://github.com/FirebirdSQL/firebird/commit/95542f0939d82dcd4c0278fb3194ff266eea4e3a
             ("Merge pull request #8896 from FirebirdSQL/work/gh-8885")
    Confirmed bug on 6.0.0.1405, 5.0.4.1756: server crashed, message 'INET/inet_error' appeared in log.
    Checked on 6.0.0.1428-06d1879; 5.0.4.1757-95542f0.
"""

import os
import re
import locale
from difflib import unified_diff
from pathlib import Path

import pytest
from firebird.qa import *

REQUIRED_ALIAS = 'tmp_gh_8885_alias'

substitutions = [ ('[ \t]+', ' ')
                ]
db = db_factory(filename = '#' + REQUIRED_ALIAS)
act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=5.0.4')
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
        set list on;
        set count on;
        select rdb$config_name, rdb$config_value from rdb$config where lower(rdb$config_name) in ('lockmemsize', 'lockhashslots');
    """

    expected_stdout = f"""
        RDB$CONFIG_NAME LockMemSize
        RDB$CONFIG_VALUE 262144
        RDB$CONFIG_NAME LockHashSlots
        RDB$CONFIG_VALUE 65521
        Records affected: 2
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

    allowed_patterns = [  re.compile('INET/inet_error',re.IGNORECASE),
                          re.compile('Error\\s+(reading|writing)',re.IGNORECASE),
                       ]

    for line in unified_diff(log_before, log_after):
        if (msg := line.strip()):
            if msg.startswith('+') and act.match_any(msg, allowed_patterns):
                print('UNEXPECTED MESSAGE IN LOG:', msg[1:])

    act.expected_stdout = """
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
