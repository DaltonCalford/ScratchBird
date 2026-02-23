#coding:utf-8

"""
ID:          issue-6650
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6650
TITLE:       Firebird is freezing when trying to manage users via triggers
DESCRIPTION:
    Confirmed hang on 4.0.0.2214

    Checked on 4.0.0.2249 - no hang, but if this test runs in loop, w/o pauses for at
    least ~4s, then starting from 2nd run following fail raises:
        Statement failed, SQLSTATE = 08006
        Error occurred during login, please check server firebird.log for details
        Error occurred during login, please check server firebird.log for details
    This was because of: http://tracker.firebirdsql.org/browse/CORE-6441 (fixed).
    Content of firebird.log will be added with following lines:
        Srp Server
        connection shutdown
        Database is shutdown.
    Sent report to Alex et al, 09.11.2020.
JIRA:        CORE-6412
FBTEST:      bugs.core_6412
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

    Confirmed again problem: 4.0.0.2214 hangs.
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
REQUIRED_ALIAS = 'tmp_core_6412_alias'


db = db_factory()

act = python_act('db', substitutions=[('\t+', ' '), ('TCPv.*', 'TCP')])

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

    init_sql = f'''
       set bail on;
       set list on;
       create database '{REQUIRED_ALIAS}' user {act.db.user};
       create user {act.db.user} password '{tmp_dba_pswd}' using plugin Srp;
       alter database set linger to 0;
       commit;

       connect 'localhost:{REQUIRED_ALIAS}' user {act.db.user} password '{tmp_dba_pswd}';

       set list on;
       select a.mon$remote_protocol,
              d.mon$sec_database
       from mon$attachments a cross join mon$database d
       where a.mon$attachment_id = current_connection;

       create table t_users
       (
         authentication varchar(32) character set win1252 collate win_ptbr
       );
       commit;

       set term ^ ;
       create trigger t_users_ai_au_ad for t_users
       active after insert or update or delete position 0
       as
       begin
        if ((old.authentication is not null) and ((new.authentication is null) or (old.authentication<>new.authentication))) then
        begin
              execute statement 'revoke rdb$admin from ' || old.authentication || ' granted by {act.db.user}';
              execute statement 'drop user ' || old.authentication || ' using plugin srp';
           end

        if ((new.authentication is not null) and ((old.authentication is null) or (old.authentication<>new.authentication))) then
        begin
               execute statement 'grant rdb$admin to ' || new.authentication || ' granted by {act.db.user}';
               execute statement 'create or alter user ' || new.authentication || ' set password ''123456'' using plugin srp grant admin role';
        end
       end^
       set term ; ^
       commit;

       insert into t_users (authentication) values ('aaa');
       commit;

       update t_users set authentication='bbb' where upper(authentication) = upper('aaa');
       commit;

       set list on;
       select sec$user_name,sec$admin,sec$plugin from sec$users; -- [WinError 22] here for recent FB 4.x and 5.x!
       quit;
    '''

    try:
        act.expected_stdout = f"""
            MON$REMOTE_PROTOCOL             TCP
            MON$SEC_DATABASE                Self

            SEC$USER_NAME                   SYSDBA
            SEC$ADMIN                       <true>
            SEC$PLUGIN                      Srp
            SEC$USER_NAME                   BBB
            SEC$ADMIN                       <true>
            SEC$PLUGIN                      Srp
        """
        act.isql(switches = ['-q'], input = init_sql, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

        act.stdout = capsys.readouterr().out
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
