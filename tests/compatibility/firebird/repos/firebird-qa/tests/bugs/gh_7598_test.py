#coding:utf-8

"""
ID:          issue-7598
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7598
TITLE:       Wrong detection of must-be-delimited user names
DESCRIPTION:
    Test uses pre-defined alias ('TMP_GH_7598_ALIAS') for which we set two parameters:
        * MaxStatementCacheSize = 32K
        * DeadlockTimeout = 1
    Also, we create common temporary database (NOT "aliased") for which MaxStatementCacheSize will be default.
    This database is used for adding initial data (otherwise problems raised with aliased database on build
    5.0.0.1057 because of very low value of MaxStatementCacheSize ==> one can not reproduce bug).
    Then this DB is moved to the file that is defined by TMP_GH_7598_ALIAS and we can perform test per se.
    After first connection if made and several rows are fetched, we launch ISQL as async child process and
    try to create table in it (see ticket example). On build before fix this ISQL can either hang or raise
    'deadlock' exception (we use DeadlockTimeout = 1 in order to reduce waiting time for that).
    On 5.0.1058 ISQL must finish without problems: table 'test2' must be created successfully.

NOTES:
    [30.05.2023] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Test uses pre-created databases.conf which has alias defined by variable REQUIRED_ALIAS.
       Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.
       Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace
       it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    3. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)

    Confirmed 'deadlock' error on 5.0.0.1057 but only when use MaxStatementCacheSize = 32K. COULD NOT reproduce hang.
    Checked on 5.0.0.1058 SS/CS -- all OK.
"""

import os
import re
import locale
import shutil
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
REQUIRED_ALIAS = 'tmp_gh_7598_alias'

DB_PAGE_SIZE = 8192
INIT_ROWS_CNT = 500000
MAX_WAIT_FOR_ISQL_FINISH = 3

db = db_factory(filename = '#' + REQUIRED_ALIAS)
db_clone = db_factory(page_size = DB_PAGE_SIZE, do_not_drop = True)

act = python_act('db', substitutions=[('[ \t]+', ' '), ] )
act_clone = python_act('db_clone')

tmp_sql = temp_file(filename = 'tmp_gh_7598.tmp.sql')
tmp_log = temp_file(filename = 'tmp_gh_7598.tmp.log')

@pytest.mark.version('>=5.0')
def test_1(act: Action, act_clone: Action, tmp_sql: Path, tmp_log: Path, capsys):

    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_7598_alias = $(dir_sampleDb)/qa/tmp_gh_7598.fdb
                # - then we extract filename: 'tmp_gh_7598.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    test_db_file = ''
    with act.db.connect(no_db_triggers = True) as con:
        con.execute_immediate('alter database set linger to 0')
        con.commit()
        test_db_file = con.info.name


    sql_init = f'''
        set bail on;
        -- set echo on;
        connect '{act_clone.db.db_path}' user {act.db.user}; --  password '{act.db.password}';
        create table test1(id int, str1 varchar(50));
        commit;
        set term ^;
        create or alter procedure gen_data(i int) as
        begin
            while (i > 0) do
            begin
                insert into test1(id, str1) values (:i, '01234567890123456789012345678901234567890123456789');
                i = i - 1;
            end
        end^

        set term ;^

        execute procedure gen_data({INIT_ROWS_CNT});
        commit;
        create index test1_id on test1 (id);
        commit;
    '''

    act.expected_stdout = ''
    act.isql(switches = ['-q'], input = sql_init, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    shutil.move( act_clone.db.db_path, test_db_file)

    with act.db.connect() as con1:
        cur1 = con1.cursor()
        for r in cur1.execute('select first 10 * from test1 where ID > 0'):
            pass
        con1.commit()

        ###################################
        # NB: do NOT close connection here!
        ###################################

        sql_test = f'''
            set bail on;
            set list on;
            -- set autoddl off;
            -- set echo on;
            connect 'localhost:{REQUIRED_ALIAS}' user {act.db.user} password '{act.db.password}';
            create table test2(id int);
            commit; -- this issued 'deadlock' before fix.
            select 1 as table_created from rdb$relations where rdb$relation_name = upper('test2');
            quit;
        '''
        tmp_sql.write_bytes(sql_test.encode('utf-8'))

        with open(tmp_log,'w') as f_log:
            p = subprocess.Popen( [ act.vars['isql'],
                                    '-q',
                                    '-i', tmp_sql
                                  ], 
                                  stdout = f_log, stderr = subprocess.STDOUT
                                )
            for i in range(MAX_WAIT_FOR_ISQL_FINISH):
                time.sleep(1)
                if p.poll():
                    break

            if not p.poll():
                p.terminate()

    for line in tmp_log.read_text().splitlines():
        if line.split():
            print(line)

    act.expected_stdout = 'TABLE_CREATED 1'
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
