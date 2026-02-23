#coding:utf-8

"""
ID:          issue-5255
ISSUE:       5255
JIRA:        CORE-4964
FBTEST:      bugs.core_4964
TITLE:       Real errors during connect to security database are hidden by Srp user manager. Errors should be logged no matter what AuthServer is used
DESCRIPTION:
    Test does following:
    1) creates temporary user using plugin Srp (in order to avoid occasional connect as SYSDBA using Legacy plugin);
    2) makes copy of test DB to file which is specified n databases.conf as database for alias defined by variable with name REQUIRED_ALIAS
       (its value: 'tmp_4964_alias'; test will try to connect to this file via ALIAS from pre-created databases.conf);
    3) uses pre-created databases.conf which has alias and SecurityDatabase parameter in its details.
       This parameter that points to existing file that for sure can NOT be a Firebird database
       (file $(dir_conf)/firebird.msg is used for this purpose).

    Then we:
    1) obtain content of server firebird.log;
    2) try to make connect to alias <tmp_alias> and (as expected) get error;
    3) obtain again content of server firebird.log and compare to origin one.

NOTES:
    [02.08.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Make sure that firebird was launched by user who is currently runs this test.
       Otherwise shutil.copy2() failes with "[Errno 13] Permission denied".
    4. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    
    [31.07.2024] pzotov
    Replaced assert for ISQL output: added diff_patterns that must filter STDERR because we have to suppress message with text
    "file ... is not a valid database" as it can be seen only in dev-builds.
    Expected ISQL output must be compared with filtered capsys.readouterr().out rather than with act.stdout
    Noted by Dimitry Sibiryakov, https://github.com/FirebirdSQL/firebird-qa/issues/27

    Checked on 5.0.0.591, 4.0.1.2692, 3.0.8.33535 - both on Windows and Linux.

    [26.02.2025] pzotov
    Old alias renamed to 'tmp_core_4964_alias_5x' - it will be used only for FB 3.x ... 5.x.
    For 6.x+: added two aliases (for Windows and Linux) in the preliminary created databases.conf: 'tmp_core_4964_alias_win' and 'tmp_core_4964_alias_nix'.
    Both contain SecurityDatabase that points to library fbSampleDbCrypt.dll (Linux: libfbSampleDbCrypt.so) which is binary file and its signature DIFFERS
    from firebird.msg which was in use before.
    The problem appeared since 6.0.0.647 (21.02.2025) when ODS was changed. Among other changes, FB does not do additional check for header, i.e.:
        *** WAS: ***
            if (header->hdr_header.pag_type != pag_header || header->hdr_sequence)
               ERR_post;
        *** NOW: ***
            if (header->hdr_header.pag_type != pag_header)
               ERR_post;
        (field 'header->hdr_sequence' no more exists).
    But firebird.msg  starting byte is 0x01 - and this is equal to pag_header, thus firebird.msg is no more suitable for this test in 6.x.
    Explained by dimitr, letter 25.02.2025 22:45
"""
import os
import re
import time
from pathlib import Path
from difflib import unified_diff

import pytest
from firebird.qa import *

substitutions = [
                    ('[ \t]+', ' ')
                   ,('(-)?file .* is not a valid database', 'file is not a valid database')
                ]

db = db_factory()
act = python_act('db', substitutions = substitutions)
tmp_user = user_factory('db', name='tmp$c4964', password='123', plugin = 'Srp')

expected_stdout_isql = """
    Statement failed, SQLSTATE = 08006
    Error occurred during login, please check server firebird.log for details
"""

expected_stdout_log_diff = """
    Authentication error
    file is not a valid database
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User, capsys):

    if act.is_version('<6'):
        REQUIRED_ALIAS = 'tmp_core_4964_alias_5x'
    else:
        REQUIRED_ALIAS = 'tmp_core_4964_alias_win' if os.name == 'nt' else 'tmp_core_4964_alias_nix'

    fblog_1 = act.get_firebird_log()

    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_4964_alias = $(dir_sampleDb)/qa/tmp_qa_4964.fdb 
                # - then we extract filename: 'tmp_qa_4964.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    # Full path + filename of database to which we will try to connect:
    #
    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )
    
    # Permiss. Error: [Errno 13] Permiss. denied --> probably because
    # Firebird was started by root rather than current (non-privileged) user.
    #
    tmp_fdb.write_bytes(act.db.db_path.read_bytes())

    check_sql = f'''
       set bail on;
       set list on;
       connect '{act.host+":" if act.host else ""}{tmp_fdb}' user {tmp_user.name} password {tmp_user.password};
       -- This can occus only if we databases.conf contains {REQUIRED_ALIAS}
       -- but without reference to invalid security DB (e.g., alias without curly braces at all):
       select mon$database_name as "UNEXPECTED CONNECTION:" from mon$database;
       quit;
    '''

    ###############################################################################################################
    # POINT-1: check that ISQL raises:
    # "SQLSTATE = 08006 / Error occurred during login, please check server firebird.log ..."
    #

    # release build:
    # ==================================
    # Statement failed, SQLSTATE = 08006
    # Error occurred during login, please check server firebird.log for details
    # ==================================

    # dev-build:
    # ==================================
    # Statement failed, SQLSTATE = 08006
    # Error occurred during login, please check server firebird.log for details
    # -file ... is not a valid database
    # ==================================
    # Last line ("file ... is not a valid database") will be suppressed by substitutions set:
    #

    isql_err_diff_patterns = [
         "Statement failed, SQLSTATE = 08006"
        ,"Error occurred during login, please check server firebird.log for details"
    ]
    isql_err_diff_patterns = [re.compile(s) for s in isql_err_diff_patterns]
    
    act.expected_stdout = expected_stdout_isql
    try:
        act.isql(switches = ['-q'], input = check_sql, connect_db=False, credentials = False, combine_output = True)
    finally:
        tmp_fdb.unlink()

    # ::: NB :::
    # Expected ISQL output must be compared with filtered capsys.readouterr().out rather than with act.stdout
    for line in act.stdout.splitlines():
        if act.match_any(line, isql_err_diff_patterns):
            print(line)

    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    time.sleep(1) # Allow content of firebird log be fully flushed on disk.
    fblog_2 = act.get_firebird_log()

    fb_log_diff_patterns = [
         "\\+\\s+Authentication error"
        ,"\\+\\s+file .* is not a valid database"
    ]
    fb_log_diff_patterns = [re.compile(s) for s in fb_log_diff_patterns]
    
    # BOTH release and dev build will print in firebird.log:
    # <hostname> <timestamp>
    # Authentication error
    # file .* is not a valid database
    #
    for line in unified_diff(fblog_1, fblog_2):
        if line.startswith('+'):
            if act.match_any(line, fb_log_diff_patterns):
                print(line.split('+')[-1])
                
    ###############################################################################################################
    # POINT-2: check that diff between firebird.log initial and current content has phrases
    # 'Authentication error' and '... file is not a valid database':
    #
    act.expected_stdout = expected_stdout_log_diff
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

