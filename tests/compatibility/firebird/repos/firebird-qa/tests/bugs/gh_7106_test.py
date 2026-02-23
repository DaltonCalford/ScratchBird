#coding:utf-8

"""
ID:          issue-7106
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7106
TITLE:       Wrong detection of must-be-delimited user names
DESCRIPTION:
    Test tries to create several users which have names containig misc delimiter characters, see 'CHECKED_NAMES'.
    After that, we make two calls of ISQL utility for each of them: one with script that contains CONNECT statement
    with specifying this user and second - when user name is specified in command-line "-user" parameter.
    None of these actions must raise any exceptions.
    Users are created in temporary self-security database defined as alias in the databases.conf, so we can escape
    need to drop them on exit.
NOTES:
    [06.03.2023] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has alias defined by variable REQUIRED_ALIAS.
       Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)

    Explanation by Alex with example: letter 06.03.2023 18:58
    Confirmed problem on 5.0.0.376 (with user name 'test-user')
    Checked on 5.0.0.379, 5.0.0.970, 4.0.3.2904, 3.0.11.33665 -- all OK.

    [31.12.2024] pzotov
    User names with delimiting character must be enclosed in double quotes since 6.0.0.570 otherwise we get
    "SQLSTATE = 08006 / Error occurred during login, please check server firebird.log for details" and firebird.log
    will contain:
        Authentication error cannot attach to password database
        Error in isc_compile_request() API call when working 
        with legacy security database table PLG$USERS is not defined

    Parsing problem appeared on 6.0.0.0.570 after d6ad19aa07deeaac8107a25a9243c5699a3c4ea1
    ("Refactor ISQL creating FrontendParser class").
"""

import os
import re
import locale
import subprocess
from pathlib import Path
import time

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

# Name of alias for self-security DB in the QA_root/files/qa-databases.conf.
# This file must be copied manually to each testing FB homw folder, with replacing
# databases.conf there:
#
REQUIRED_ALIAS = 'tmp_gh_7106_alias'

db = db_factory(filename = '#' + REQUIRED_ALIAS, do_not_create = True, do_not_drop = True )
act = python_act('db', substitutions=[('[ \t]+', ' '), ] )

@pytest.mark.version('>=3.0.10')
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
                #     tmp_7106_alias = $(dir_sampleDb)/qa/tmp_gh_7106.fdb
                # - then we extract filename: 'tmp_gh_7106.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    # Full path + filename of database to which we will try to connect:
    #
    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )
    tmp_fdb.unlink(missing_ok=True)

    sql_init = f'''
        set bail on;
        set list on;
        set blob all;
        create database '{REQUIRED_ALIAS}' user {act.db.user} password '{act.db.password}';
        create user {act.db.user} password '{act.db.password}' using plugin Srp;
        select -- m.mon$database_name,
            m.mon$sec_database as mon_sec_db
        from mon$database m;
        commit;
    '''

    expected_stdout_isql = f'''
        MON_SEC_DB Self
    '''

    act.expected_stdout = expected_stdout_isql
    act.isql(switches = ['-q'], input = sql_init, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()


    CHECKED_NAMES = \
    ( 'test-user',
     '#',
     '-',
     '@',
     '$',
     '?',
     '-?',
     '=',
     '.',
     ',',
     ':',
     'localhost:',
     '-a',
     '-b',
     '-c',
     '-ch',
     '-e',
     '-ex',
     '-f',
     '-i',
     '-m',
     '-n',
     '-nod',
     '-now',
     '-o',
     '-p',
     '-q',
     '-r',
     '-r2',
     '-s',
     '-t',
     '-tr',
     '-u',
     '-x',
     '-z',
    )
    # Following names seems to be not allowed or cause problems:
    # ; % * --

    
    with act.db.connect() as con:
        cur = con.cursor()
        #for r in cur.execute('select d.mon$database_name, a.mon$remote_protocol from mon$database d cross join mon$attachments a where a.mon$attachment_id = current_connection'):
        #    print(r[0], r[1])
        
        for u in CHECKED_NAMES:
            try:
                con.execute_immediate(f"""create or alter user "{u}" password '123' using plugin Srp""")
                con.commit()
            except DatabaseError as e:
                print('Could not create user "{u}":')
                print(e.__str__())
                print(e.sqlcode)
                for g in e.gds_codes:
                    print(g)

        act.expected_stdout = ''
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()


    for use_connect_sttm in (True,False,):
        for u_name in CHECKED_NAMES:
            isql_connect_sttm = \
                '\n'.join( ( 'set bail on;',
                             'set heading off;',
                             f"""connect 'localhost:{REQUIRED_ALIAS}' user "{u_name}" password '123';""" if use_connect_sttm else "",
                             'select mon$user from mon$attachments a where a.mon$attachment_id = current_connection;'
                           )
                         )

            act.expected_stdout = u_name
            if use_connect_sttm:
                # Try to make connection using isql CONNECT operator:
                act.isql(switches = ['-q'], input = isql_connect_sttm, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                # try to make connection via command-line argument "-user ...":
                act.isql(switches = ['-q', '-user', f'"{u_name}"', '-pas', '123', f'localhost:{REQUIRED_ALIAS}'], input = isql_connect_sttm, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
            assert act.clean_stdout == act.clean_expected_stdout, f'User "{u_name}" could not make connection using isql {"" if use_connect_sttm else " -user ... -pas ..."} and script:\n{isql_connect_sttm}'
            act.reset()

    act.gfix(switches = [ '-shut', 'full', '-force', '0', f'localhost:{REQUIRED_ALIAS}', '-user', act.db.user, '-pas', act.db.password ], credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    tmp_fdb.unlink()
