#coding:utf-8

"""
ID:          issue-7718
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7718
TITLE:       Allow to create database with different owner
DESCRIPTION:
    Test runs 'CREATE DATABASE' with specifying OWNER clause with temporary user who has no any privilege.
    Then we try to connect to just created database by this user.
    He must OWNER of this DB and thus have ability to run any DDL there (only 'create table' is checked).
    Finally, this user must be able to DROP database.
NOTES:
    Currently test checks only first option of following three:
       Allow to create database with different owner (#7718)
       Allow to create database with different owner via API
       Support for OWNER clause in executeCreateDatabase()
    Checked on 6.0.0.139
"""
from pathlib import Path

import pytest
from firebird.qa import *

db = db_factory()
#tmp_fdb = db_factory(filename = 'tmp_gh_7200.tmp.fdb')

tmp_fdb = temp_file(filename = 'tmp_gh_7718.tmp.fdb')
tmp_own = user_factory('db', name = 'tmp$7718_owner', password = '123')

act = python_act('db', substitutions=[('[ \t]+',' ')] )

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_fdb: Path, tmp_own: User):

    test_script = f"""
        set bail on;
        set list on;
        create database 'localhost:{tmp_fdb}' owner {tmp_own.name} user {act.db.user} password '{act.db.password}';
        commit;
        connect 'localhost:{tmp_fdb}' user {tmp_own.name} password '{tmp_own.password}';
        select current_user as whoami from rdb$database;
        create table test_owned_by_non_sysdba(id int);
        commit;
        select mon$owner from mon$database;
        select rdb$owner_name from rdb$relations where rdb$relation_name = upper('test_owned_by_non_sysdba');
        commit;
        drop database;
    """

    expected_stdout = f"""
        WHOAMI         {tmp_own.name.upper()}
        MON$OWNER      {tmp_own.name.upper()}
        RDB$OWNER_NAME {tmp_own.name.upper()}
    """

    act.expected_stdout = expected_stdout
    act.isql(switches=['-q'], input = test_script, combine_output = True, connect_db = False, credentials = False)
    assert act.clean_stdout == act.clean_expected_stdout
