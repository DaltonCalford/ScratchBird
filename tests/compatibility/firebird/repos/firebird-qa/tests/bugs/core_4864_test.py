#coding:utf-8

"""
ID:          issue-5160
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5160
TITLE:       CREATE DATABASE fail with ISQL
DESCRIPTION:
    Test uses pre-created databases.conf which has alias (see variable REQUIRED_ALIAS) and SecurityDatabase in its details
    which points to that alias, thus making such database be self-security.
    Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
    
    Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
    (for LINUX this equality is case-sensitive, even when aliases are compared!)

    We connect to currently used test DB (which is created by QA plugin) via local protocol, using appropriate
    'CONNECT' command in the SQL script.
    Then we attempt to create new database using command which was providd in the ticket (only page_size was increased to recent).
    After this, we create user in this (new) database and ensure that this DB has owner = current OS user, is self-security etc.
    Finally, we drop new DB and make connect to 'previous' DB which was created by QA plugin, and check that there is NO user
    which was created in the self-security (just dropped) DB.

JIRA:        CORE-4864
FBTEST:      bugs.core_4864
NOTES:
    [04.08.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    4. It is crucial to be sure that current OS environment has no ISC_USER and ISC_PASSWORD variables. Test forcibly unsets them.

    Confirmed problem on WI-T3.0.0.31896 Firebird 3.0 Beta 2 (date of snapshot: 02-JUL-2015), but only when
    using ISQL (i.e. like it was specified in the ticked: 'isql employee -user sysdba').
    Current version of firebird-driver can not work with 3.0.0.31896, internal error raises before any connect can be established.

    Problematic statement:
        create database 'tmp_core_4864_alias' page_size 8192 default character set win1250 collation pxw_hun;
        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -104
        -Token unknown - line 1, column 45
        -sysdba
    If we use script with "CONNECT 'employee' user SYSDBA;" (instead of isql employee -user sysdba) then error looks differ:
        Statement failed, SQLSTATE = 42S02
        find/display record error
        -table PLG$VIEW_USERS is not defined
    (and it raises when we run query 'select ... from mon$database join mon$attachments ... left join sec$users ...').

    [20.09.2022] pzotov
    Removed query to security.db ("select s.sec$user_name as remained_in_main_sec_db ...") because it can cause translit error
    in case when some non-ascii user remains in this database (after previous tests). This problem is quite irrelevant to this test.

    Checked on 5.0.0.591, 4.0.1.2692, 3.0.8.33535 - both on Windows and Linux.
"""

import os
import locale
import re
from pathlib import Path
import getpass

import pytest
from firebird.qa import *

substitutions = [('[ \t]+', ' '), ]

REQUIRED_ALIAS = 'tmp_core_4864_alias'

# MANDATORY! OTHERWISE ISC_ variables will take precedense over credentials = False!
for v in ('ISC_USER','ISC_PASSWORD'):
    try:
        del os.environ[ v ]
    except KeyError as e:
        pass

db = db_factory()
act = python_act('db', substitutions=substitutions)


@pytest.mark.version('>=3.0')
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
    tmp_usr = 'tmp$user_4864'
    
    check_sql = f"""
        set bail on;
        set list on;

        -- XXX WRONG XXX >>> connect '{act.db.db_path}' user {act.db.user};
        -- WE HAVE TO ESTABLISH CONNECTION FROM COMMAND LINE, USING '-user' switch!

        select upper(mon$database_name) as initial_db_name, a.mon$remote_protocol as initial_protocol, a.mon$user as initial_user
        from mon$database join mon$attachments a on a.mon$attachment_id = current_connection;
        rollback;

        create database '{REQUIRED_ALIAS}' page_size 8192 default character set win1250 collation pxw_hun;
        commit;
        connect '{REQUIRED_ALIAS}' user {act.db.user};
        create user {tmp_usr} password '123' using plugin Srp;
        commit;
        connect '{REQUIRED_ALIAS}' user {tmp_usr};
        select
            upper(m.mon$database_name) as new_db_name
            ,upper(m.mon$owner) as new_db_owner
            ,m.mon$sec_database as new_db_sec_database
            ,a.mon$user as new_db_user
            ,s.sec$user_name
            ,s.sec$plugin
        from mon$database m
        join mon$attachments a on a.mon$attachment_id = current_connection
        left join sec$users s on a.mon$user = s.sec$user_name
        ;
        commit;
        connect '{REQUIRED_ALIAS}' user {act.db.user};
        drop database;
        quit;
    """

    expected_stdout = f"""
        INITIAL_DB_NAME {str(act.db.db_path).upper()}
        INITIAL_PROTOCOL <null>
        INITIAL_USER {act.db.user}
        NEW_DB_NAME {str(tmp_fdb).upper()}
        NEW_DB_OWNER {act.db.user}
        NEW_DB_SEC_DATABASE Self
        NEW_DB_USER {tmp_usr.upper()}
        SEC$USER_NAME {tmp_usr.upper()}
        SEC$PLUGIN Srp
    """
    
    act.expected_stdout = expected_stdout

    # WRONG >>> act.isql(switches=['-q'], charset = 'utf8', input = check_sql, credentials = False, connect_db = False)
    # We have to run ISQL exactly how it was shown in the ticket, i.e. 'isql -q -user SYSDBA'.
    # If we run ISQL without '-user ....' switch and make connection to test DB in the .sql script (using CONNECT command)
    # then error (on 3.0.0.31896 Beta 2) will be in another statement and it will be different: SQLSTATE = 42S02.
    # Also, NEW_DB_OWNER will have name of current OS user rather than SYSDBA.
    #
    act.isql(switches=['-q', act.db.db_path, '-user', act.db.user], charset = 'win1250', input = check_sql, credentials = False, connect_db = False, combine_output = True, io_enc = locale.getpreferredencoding())

    assert act.clean_stdout == act.clean_expected_stdout # and act.clean_stderr == act.clean_expected_stderr
    act.reset()
