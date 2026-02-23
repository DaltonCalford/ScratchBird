#coding:utf-8

"""
ID:          issue-6392
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6392
TITLE:       Error 'Multiple maps found for ...' is raised in not appropriate case
DESCRIPTION:
    There was issue about mapping of ROLES: currently mapping can be done only for trusted role.
    But documentation does not mention about this. One can conclude that mapping should work
    as for trusted role and also for "usual" way (i.e. when used specifies 'ROLE ...' clause).
    Discussion about this with Alex was in 23-sep-2019, and his solution not yet known.
    For this reason it was decided to REMOVE code that relates tgo ROLE mapping in this test.
JIRA:        CORE-6143
FBTEST:      bugs.core_6143
NOTES:
    [03.11.2021] pcisar
    This test fails for 4.0, WHO_AM_I = TMP$C6143_FOO instead global_mapped_user

    [22.09.2022] pzotov
    0. Totally reimplemented. Test uses self-security database by creating its alias that is defined
       in the pre-created databases.conf (together with Auth* and UserManager plugins).
       This removes any problems that can raise with global mappings because they are created actually
       in the temporary FDB file rather than in default security DB.
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    Checked on Linux and Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS)
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
REQUIRED_ALIAS = 'tmp_core_6143_alias'

tmp_user = user_factory('db', name='tmp$c6143_foo', password='123', plugin='Srp')

db = db_factory()
act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action, tmp_user: User, capsys):

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

    tmp_dba_pswd = 'alterkey'
    USER_PLUGIN = 'Srp'
    AUTH_PLUGIN = 'Srp'

    sql_txt = f"""
        set bail on;
        set wng off;
        set list on;
        -- set echo on;
        create database '{REQUIRED_ALIAS}' user {act.db.user};
        create user {act.db.user} password '{tmp_dba_pswd}' using plugin {USER_PLUGIN};

        select
            m.mon$sec_database as mon_sec_db
        from mon$database m;
        commit;

        create or alter user {tmp_user.name} password '{tmp_user.password}' using plugin {USER_PLUGIN};
        commit;

        -- ++++++++++++++++++++++++ T E S T    L O C A L    M A P P I N G  +++++++++++++++++++++++

        create or alter mapping local_map_for_user_1 using plugin {AUTH_PLUGIN} from user {tmp_user.name} to user local_mapped_user;
        create or alter mapping local_map_for_user_2 using plugin {AUTH_PLUGIN} from user {tmp_user.name} to user local_mapped_user;
        commit;

        connect 'localhost:{REQUIRED_ALIAS}' user {tmp_user.name} password '{tmp_user.password}';

        select
            'Connected OK when local mapping is duplicated.' as msg
            ,current_user as who_am_i     -- <<< 'local_mapped_user' must be shown here, *NOT* {tmp_user.name}
        from rdb$database;
        commit;

        -- for SELF-SECURITY database we have to DROP local mappings
        -- otherwise subsequent connect will fail with 08004 (multiple mappings):
        connect 'localhost:{REQUIRED_ALIAS}' user {act.db.user} password '{tmp_dba_pswd}';
        drop mapping local_map_for_user_1;
        drop mapping local_map_for_user_2;
        commit;

        -- ++++++++++++++++++++++++ T E S T    G L O B A L    M A P P I N G  +++++++++++++++++++++++

        create or alter global mapping global_map_for_user_1 using plugin {AUTH_PLUGIN} from user {tmp_user.name} to user global_mapped_user;
        create or alter global mapping global_map_for_user_2 using plugin {AUTH_PLUGIN} from user {tmp_user.name} to user global_mapped_user;
        commit;

        connect 'localhost:{REQUIRED_ALIAS}' user {tmp_user.name} password '{tmp_user.password}';

        select
            'Connected OK when global mapping is duplicated.' as msg
            ,current_user as who_am_i     -- <<< 'global_mapped_user' must be shown here, *NOT* {tmp_user.name}
        from rdb$database;
        commit;

    """

    try:
        act.expected_stdout = """
            MON_SEC_DB                      Self
            MSG                             Connected OK when local mapping is duplicated.
            WHO_AM_I                        LOCAL_MAPPED_USER
            MSG                             Connected OK when global mapping is duplicated.
            WHO_AM_I                        GLOBAL_MAPPED_USER
        """
        act.isql(switches = ['-q'], input = sql_txt, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
        
    finally:
        if Path.exists(tmp_fdb):
            # Change DB state to full shutdown in order to have ability to drop database file.
            # This is needed because when DB is self-security then it will be kept opened for 10s
            # (as it always occurs for common security.db). Set linger to 0 does not help.
            # Attempt to use 'drop database' fails with:
            # "SQLSTATE = 40001 / lock time-out on wait transaction / -object ... is in use"
            act.gfix(switches=['-shut', 'full', '-force', '0', f'localhost:{REQUIRED_ALIAS}', '-user', act.db.user, '-pas', tmp_dba_pswd], io_enc = locale.getpreferredencoding(), credentials = False, combine_output = True)
            tmp_fdb.unlink()

            act.stdout = capsys.readouterr().out
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()
