#coding:utf-8

"""
ID:          issue-6142
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6142
TITLE:       Initial global mapping from srp plugin does not work
DESCRIPTION:
JIRA:        CORE-5884
FBTEST:      bugs.core_5884
NOTES:
    [07.02.2022] pcisar
      Test fails on 4.0.1 because CURRENT_USER name is not from mapping, but mapped user.
      Can't judge whether it's ok for v4, or regression from 3.0.4

    [15.09.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has alias defvined by variable REQUIRED_ALIAS.
       Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)

    4. Outcome of this test (pass/fail) depends on whether *per-database* 'AuthServer' parameter is used for test DB!
       Pre-created section in databases.conf must contain 'AuthServer' parameter. Its its value must be the EXACTLY
       the same as we set here to setting PLUGIN_FOR_MAPPING (e.g. if it is 'Srp256' then it must be specified both
       in databases.conf section and here in PLUGIN_FOR_MAPPING variable).
       Otherwise user name from DPB will be shown instead of mapping ('tmp$c5884_*' instead of '(global|local)_mapped_*')
       Sent report to Alex et al, 15.09.2022.

       !! NB !! Default values for Auth* parameters differ in 3.x vs 4.x+, at least in ORDER of their appearance:
           3.0.11.x:
             AuthClient = Srp, Srp256, Legacy_Auth
             AuthServer = Srp

           4.x and 5.x:
             AuthClient = Srp256, Srp, Legacy_Auth
             AuthServer = Srp256
    Checked on Linux and Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.730
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
REQUIRED_ALIAS = 'tmp_core_5884_alias'

db = db_factory()
act = python_act('db') # , substitutions=[('[ \t]+', ' '), ('.*===.*', ''), ('PLUGIN .*', 'PLUGIN')])

user_a = user_factory('db', name='tmp$c5884_1', password='123', plugin='Srp', do_not_create = True)
user_b = user_factory('db', name='tmp$c5884_2', password='456', plugin='Srp', do_not_create = True)

@pytest.mark.version('>=3.0.4')
def test_1(act: Action, user_a: User, user_b: User, capsys):

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

    #------------------------------------------------------------------
    # Full path + filename of database to which we will try to connect:
    #
    tmp_fdb = Path( act.vars['sample_dir'], 'qa', fname_in_dbconf )

    #######!!! ACHTUNG !!!#######
    PLUGIN_FOR_MAPPING = 'Srp256' # <<< !! MUST BE EXACTLY THE SAME AS IN databases.conf !!
    #############################

    PLUGIN_FOR_AUTH = 'Srp'
    LOCAL_MAPPING_NAME =  'local_mapping_5884'
    GLOBAL_MAPPING_NAME = 'global_mapping_5884'
    LOCAL_MAPPED_USER =   'john_from_local_mapping_5884'
    GLOBAL_MAPPED_USER =  'mike_from_global_mapping_5884'

    tmp_dba_pswd = 'alt@pa$5884'
    sql_txt = f'''
        --set echo on;
        -- set bail on;
        set list on;
        create database '{REQUIRED_ALIAS}' user {act.db.user};
        create user {act.db.user} password '{tmp_dba_pswd}' using plugin {PLUGIN_FOR_AUTH};

        create user {user_a.name} password '{user_a.password}' using plugin {PLUGIN_FOR_AUTH};
        create user {user_b.name} password '{user_b.password}' using plugin {PLUGIN_FOR_AUTH};
        commit;

        create or alter mapping {LOCAL_MAPPING_NAME} using plugin {PLUGIN_FOR_MAPPING} from user {user_a.name} to user {LOCAL_MAPPED_USER};
        create or alter global mapping {GLOBAL_MAPPING_NAME} using plugin {PLUGIN_FOR_MAPPING} from user {user_b.name} to user {GLOBAL_MAPPED_USER};
        commit;

        recreate view v_map_info as
        select t.*
        from
        (
            select
                 rdb$map_name      as map_name
                ,rdb$map_plugin    as map_plugin
                ,rdb$map_from_type as from_type
                ,rdb$map_from      as map_from
                ,rdb$map_to_type   as to_type
                ,rdb$map_to        as map_to
            from rdb$auth_mapping
        ) t
        where
            t.map_name in ( upper('{LOCAL_MAPPING_NAME}'), upper('{GLOBAL_MAPPING_NAME}') )
            and upper(t.map_plugin) = upper('{PLUGIN_FOR_MAPPING}')
        order by map_name, from_type, map_from, to_type, map_to
        ;
        commit;

        set count on;

        select m.mon$sec_database as mon_sec_db from mon$database m;

        select sec$user_name,sec$admin,sec$plugin from sec$users order by 1;
        commit;

        select * from v_map_info;
        commit;

        connect 'localhost:{REQUIRED_ALIAS}' user {user_a.name} password '{user_a.password}';
        select current_user as whoami_a from rdb$database;
        commit;

        connect 'localhost:{REQUIRED_ALIAS}' user {user_b.name} password '{user_b.password}';
        select current_user as whoami_b from rdb$database;
        commit;
    '''

    act.expected_stdout = f"""
        MON_SEC_DB                      Self
        Records affected: 1

        SEC$USER_NAME                   {act.db.user}
        SEC$ADMIN                       <true>
        SEC$PLUGIN                      Srp

        SEC$USER_NAME                   {user_a.name.upper()}
        SEC$ADMIN                       <false>
        SEC$PLUGIN                      Srp

        SEC$USER_NAME                   {user_b.name.upper()}
        SEC$ADMIN                       <false>
        SEC$PLUGIN                      Srp
        Records affected: 3

        MAP_NAME                        {GLOBAL_MAPPING_NAME.upper()}
        MAP_PLUGIN                      {PLUGIN_FOR_MAPPING.upper()}
        FROM_TYPE                       USER
        MAP_FROM                        {user_b.name.upper()}
        TO_TYPE                         0
        MAP_TO                          {GLOBAL_MAPPED_USER.upper()}

        MAP_NAME                        {LOCAL_MAPPING_NAME.upper()}
        MAP_PLUGIN                      {PLUGIN_FOR_MAPPING.upper()}
        FROM_TYPE                       USER
        MAP_FROM                        {user_a.name.upper()}
        TO_TYPE                         0
        MAP_TO                          {LOCAL_MAPPED_USER.upper()}

        Records affected: 2

        WHOAMI_A                        {LOCAL_MAPPED_USER.upper()}
        Records affected: 1

        WHOAMI_B                        {GLOBAL_MAPPED_USER.upper()}
        Records affected: 1
    """
    try:
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
