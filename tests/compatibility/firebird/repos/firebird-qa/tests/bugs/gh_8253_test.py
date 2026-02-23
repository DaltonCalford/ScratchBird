#coding:utf-8

"""
ID:          issue-8253
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8253
TITLE:       Incorrect handling of non-ASCII object names in CREATE MAPPING statement
DESCRIPTION:
    Test uses pre-created databases.conf which has alias (see variable REQUIRED_ALIAS) and SecurityDatabase in its details
    which points to that alias, thus making such database be self-security.
    Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
    Self-security database allows us to create GLOBAL mapping without worrying about how it will be removed on test finish.
NOTES:
    [23.09.2024] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    4. Confirmed bug on 6.0.0.461, data from rdb$auth_mapping.rdb$map_from:
       * is displayed as '???...???' for global mapping;
       * is mojibake for local mapping when group name is enclosed in double quotes.

    Checked on 6.0.0.466, 5.0.2.1513, 4.0.6.3156
"""

import re
from pathlib import Path

import pytest
from firebird.qa import *

REQUIRED_ALIAS = 'tmp_gh_8253_alias'

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

tmp_sql = temp_file('tmp_8253_non_ascii_ddl.sql')
tmp_log = temp_file('tmp_8253_non_ascii_ddl.log')

@pytest.mark.intl
@pytest.mark.version('>=4.0.6')
def test_1(act: Action, tmp_sql: Path, tmp_log: Path, capsys):

    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_8253_alias = $(dir_sampleDb)/qa/tmp_qa_8253.fdb 
                # - then we extract filename: 'tmp_qa_8253.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    dba_pswd = 'alterkey'
    g_name = 'лондонский симфонический оркестр'
    r_name = 'настройщик роялей'
    non_ascii_ddl = f'''
        set list on;
        create database '{REQUIRED_ALIAS}' user {act.db.user};
        create user {act.db.user} password '{dba_pswd}';
        commit;
        connect 'localhost:{REQUIRED_ALIAS}' user {act.db.user} password '{dba_pswd}';
        select mon$sec_database from mon$database; -- must be: 'Self'
        create role "{r_name}";
        commit;
        create mapping "локальная_апостроф" using any plugin from group '{g_name}' to role "{r_name}";
        create mapping "локальная_кавычки" using any plugin from group "{g_name}" to role "{r_name}";
        create global mapping "глобальная_апостроф" using any plugin from group '{g_name}' to role "{r_name}";
        create global mapping "глобальная_кавычки" using any plugin from group "{g_name}" to role "{r_name}";
        commit;
        set count on;
        select rdb$map_name,rdb$map_from,rdb$map_to from rdb$auth_mapping order by rdb$map_name;
        commit;
    '''

    tmp_sql.write_bytes(non_ascii_ddl.encode('cp866'))
    act.isql(switches=['-q'], input_file=tmp_sql, credentials = False, connect_db = False, combine_output = True, charset='dos866', io_enc = 'cp866')
    tmp_log.write_bytes(act.clean_stdout.encode('utf-8'))
    with open(tmp_log, 'r', encoding = 'utf-8', errors = 'backslashreplace') as f:
        for line in f:
            print(line)

    act.expected_stdout = f"""
        MON$SEC_DATABASE                Self
        RDB$MAP_NAME                    глобальная_апостроф
        RDB$MAP_FROM                    {g_name}
        RDB$MAP_TO                      {r_name}
        RDB$MAP_NAME                    глобальная_кавычки
        RDB$MAP_FROM                    {g_name}
        RDB$MAP_TO                      {r_name}
        RDB$MAP_NAME                    локальная_апостроф
        RDB$MAP_FROM                    {g_name}
        RDB$MAP_TO                      {r_name}
        RDB$MAP_NAME                    локальная_кавычки
        RDB$MAP_FROM                    {g_name}
        RDB$MAP_TO                      {r_name}
        Records affected: 4
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout

