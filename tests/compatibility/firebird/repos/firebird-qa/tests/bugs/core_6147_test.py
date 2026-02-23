#coding:utf-8

"""
ID:          issue-6396
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6396
TITLE:       PLG$SRP table, PLG$SRP_VIEW View instructions are strangely added in the metadata script extracted when Windows trusted authentication is enabled
DESCRIPTION:

    ## ACHTUNG ## Ticket title ("PLG$SRP table, PLG$SRP_VIEW View instructions are strangely added ...")
    must be REPLACED because references to the table PLG$SRP and view PLG$SRP_VIEW *always* present
    in extracted metadata, and this is EXPECTED, i.e. this is not error!
    The only change that was in the FB source related to this ticket is appearance of 'OR ALTER'
    phrase in the 'CREATE [OR ALTER] GLOBAL MAPPING ...' statement.
    (i.e. phase "OR ALTER" missed in extracted metadata before that).

    Builds before 4.0.0.2084 did not add this clause in extracted metadata script (checked 4.0.0.2076).
    COnfirmed problem also on 3.0.5.33115 (date of build: 26-mar-2019).

    Comparison between major FB and builds (3.0.5.33212 vs 3.0.6.33326 and 4.0.0.2076 vs 4.0.0.2084):
    see letter to Alex, 01-JUL-2020 21:38. Reply from Alex: 02-JUL-2020 08:23.

    fix for FB 4.x
    https://github.com/FirebirdSQL/firebird/commit/dbc28a88c0b96964c19f9a8fa76f7f3dc1db16c4
    Date: 01-Jul-2020 18:06
    Changed paths: M src/isql/show.epp
    Partial f_ix for CORE-6147: be smart when dealing with global mappings in metadata script

    fix for FB 3.x
    https://github.com/FirebirdSQL/firebird/commit/66499fdbeed7fe2e1765a01e7d599553c3330b0e
    Date: 09-Jul-2020 18:10
    Changed paths: M src/isql/show.epp
    Backported f_ix for CORE-6147: script with extracted metadata may fail on global mapping

    Kind of auth plugin does not matter (i.e. win_sspi or srp), so Srp was selected for running this test
    both on Windows and Linux.

    Test uses pre-created databases.conf which has alias (see variable REQUIRED_ALIAS) and SecurityDatabase in its details
    which points to that alias, thus making such database be self-security.
    Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.

    Self-security database allows us to create GLOBAL mapping without worrying about how it will be removed on test finish.

    Checked on:
    * Windows: 4.0.0.2377 SS/CS (done for both win_sspi and Srp, but only win_sspi is used in this test for Windows)
    * Linux:   4.0.0.2377 SS/CS (done for Srp)
JIRA:        CORE-6147
FBTEST:      bugs.core_6147
NOTES:
    [07.08.2022] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"

    ::: NOTE :::
    TEST CAN AND MUST BE SIMPLIFIED.
    There is mapping_factory() in the QA plugin, and it can be used to create/drop GLOBAL mapping.
    To be discussed with pcisar.

    Checked on 5.0.0.591, 4.0.1.2692, 3.0.8.33535 - both on Windows and Linux.
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
REQUIRED_ALIAS = 'tmp_core_6147_alias'

db = db_factory()
act = python_act('db', substitutions=[('[ \t]+', ' '), ('.*===.*', ''), ('PLUGIN .*', 'PLUGIN')])

fn_meta_log = temp_file('tmp_core_6147-meta.log')
fn_meta_err = temp_file('tmp_core_6147-meta.err')

#@pytest.mark.version('>=3.0.7')
@pytest.mark.version('>=3.0')
def test_1(act: Action, fn_meta_log: Path, fn_meta_err: Path, capsys):
    
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

    PLUGIN_FOR_MAPPING = 'Srp'
    MAPPING_NAME = 'trusted_auth_c6147'

    sql_txt = f'''
        set bail on;
        set list on;
        create database '{REQUIRED_ALIAS}' user {act.db.user};
        select
            m.mon$sec_database as mon_sec_db
        from mon$database m;
        commit;

        create or alter global mapping {MAPPING_NAME} using plugin {PLUGIN_FOR_MAPPING} from any user to user;
        commit;

        recreate view v_map_info as
        select
            map_name
           ,map_type
           -- ,map_plugin
           ,from_type
           ,map_from
           ,to_type
           ,map_to
        from
        (
            select
                 'LOCAL'           as map_type
                ,rdb$map_name      as map_name
                ,rdb$map_plugin    as map_plugin
                ,rdb$map_from_type as from_type
                ,rdb$map_from      as map_from
                ,rdb$map_to_type   as to_type
                ,rdb$map_to        as map_to
            from rdb$auth_mapping
            UNION ALL
            select
                 'GLOBAL'
                ,sec$map_name
                ,sec$map_plugin
                ,sec$map_from_type
                ,sec$map_from
                ,sec$map_to_type
                ,sec$map_to
            from sec$global_auth_mapping
        ) t
        where
            t.map_name = upper('{MAPPING_NAME}')
            and upper(t.map_plugin) = upper('{PLUGIN_FOR_MAPPING}')
        ;
        commit;

        set count on;
        select * from v_map_info;
        quit;
    '''

    expected_stdout_isql = f'''
        MON_SEC_DB Self

        MAP_NAME TRUSTED_AUTH_C6147
        MAP_TYPE LOCAL
        FROM_TYPE USER
        MAP_FROM *
        TO_TYPE 0
        MAP_TO <null>

        MAP_NAME TRUSTED_AUTH_C6147
        MAP_TYPE GLOBAL
        FROM_TYPE USER
        MAP_FROM *
        TO_TYPE 0
        MAP_TO <null>
        Records affected: 2
    '''
    try:
        act.expected_stdout = expected_stdout_isql
        act.isql(switches = ['-q'], input = sql_txt, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())

        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
        
        with fn_meta_log.open(mode='w') as meta_out, fn_meta_err.open(mode='w') as meta_err:
            # could not find how properly call act.extract_meta with ANOTHER database (different from currently created).
            subprocess.call( [ act.vars['isql'],'-x', '-user', act.db.user, '-pas', act.db.password, REQUIRED_ALIAS ], 
                             stdout = meta_out,
                             stderr = meta_err
                           )
        for g in (fn_meta_log, fn_meta_err):
            with g.open() as f:
                for line in f:
                    if line.split():
                        if g == fn_meta_log:
                            if ' MAPPING ' in line:
                                print(f'{line}')
                        else:
                            print(f'UNEXPECTED MATADATA STDOUT: {line}')

        act.expected_stdout = f"""
            CREATE MAPPING {MAPPING_NAME.upper()} USING PLUGIN
            CREATE OR ALTER GLOBAL MAPPING {MAPPING_NAME.upper()} USING PLUGIN
        """

    finally:
        tmp_fdb.unlink()
    
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
