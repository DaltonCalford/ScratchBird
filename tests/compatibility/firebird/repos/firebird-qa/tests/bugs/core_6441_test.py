#coding:utf-8

"""
ID:          issue-2367
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/2367
TITLE:       Srp plugin keeps connection after database has been removed for ~10 seconds (SS and SC)
DESCRIPTION:
    Test is based on CORE-6412, but instead of complex DDL it only creates DB and makes connection.
    Then it leaves ISQL and 'main' code changes DB state to full shutdown and removes database.
    After this, test immediatelly starts next iteration with the same actions. All of them must pass.
    Total number of iterations = 3.

    Confirmed bug on 4.0.0.2265:
       Statement failed, SQLSTATE = 08006
       Error occurred during login, please check server firebird.log for details

    NB. Content of firebird.log will be added with following lines:
       Srp Server
       connection shutdown
       Database is shutdown.
    This is expected (got reply from Alex, e-mail 19.11.2020 11:12).

   Checked on 3.0.8.33392 SS/SC, 4.0.0.2269 SC/SS - all fine.

JIRA:        CORE-6441
FBTEST:      bugs.core_6441
NOTES:
    [17.08.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has alias (see variable REQUIRED_ALIAS)
       and SecurityDatabase in its details which points to that alias, thus making such
       database be self-security. Database file for that alias must NOT exist in the
       QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)

    Confirmed again problem: 4.0.0.2265 (SS), got SQLSTATE = 08006 on second iteration.
    Checked on 5.0.0.623, 4.0.1.2692, 3.0.8.33535 - both on Windows and Linux; SC and SC.
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
REQUIRED_ALIAS = 'tmp_core_6441_alias'

db = db_factory()

act = python_act('db')

ITER_NUM=3

@pytest.mark.version('>=3.0.8')
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

    tmp_dba_pswd = 'QweRty6412'
    for i in range(0,ITER_NUM):
        init_sql = f'''
           set bail on;
           set list on;

           create database '{REQUIRED_ALIAS}' user {act.db.user};

           select {i} as iteration_no, 'DB creation completed OK.' as msg from rdb$database;
           alter database set linger to 0;
           create user {act.db.user} password '{tmp_dba_pswd}' using plugin Srp;
           commit;

           connect 'localhost:{REQUIRED_ALIAS}' user {act.db.user} password '{tmp_dba_pswd}';

           select {i} as iteration_no, 'Remote connection established OK.' as msg from rdb$database;

           set term ^;
           execute block as
               declare v_dummy varchar(20);
           begin
               select left(a.mon$remote_protocol,3) as mon_protocol
               from mon$attachments a
               where a.mon$attachment_id = current_connection
               into v_dummy;
           end
           ^
           set term ;^
           quit;
        '''

        try:
            act.expected_stdout = f"""
                ITERATION_NO                    {i}
                MSG                             DB creation completed OK.
                ITERATION_NO                    {i}
                MSG                             Remote connection established OK.
            """
            act.isql(switches = ['-q'], input = init_sql, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()

            # Change DB state to full shutdown in order to have ability to drop database file.
            # This is needed because when DB is self-security then it will be kept opened for 10s
            # (as it always occurs for common security.db). Set linger to 0 does not help.
            act.gfix(switches=['-shut', 'full', '-force', '0', f'localhost:{REQUIRED_ALIAS}', '-user', act.db.user, '-pas', tmp_dba_pswd], io_enc = locale.getpreferredencoding(), credentials = False, combine_output = True)
            act.stdout = capsys.readouterr().out
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()
        finally:
            tmp_fdb.unlink()
