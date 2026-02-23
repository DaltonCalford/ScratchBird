#coding:utf-8

#coding:utf-8

"""
ID:          issue-8391
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8391
TITLE:       gbak: Incorrect total statistics for restore
DESCRIPTION:
    Test uses pre-created databases.conf which has alias (see variable REQUIRED_ALIAS) with DefaultDbCachePages = 128.
    Database file for that alias must NOT exist in the QA_root/files/qa/ subdirectory: it will be created here.

    We add data to test DB (script was taken from gh_8394_test.py - it is enough to  use it instead of bug DB from ticket).
    Then we make backup and start restore with '-v -st w' option.
    Parsing of restore log must give us total number of writes that were done, see variable 'gbak_total_writes'.
    Value of gbak_total_writes must be NOT LESS than ratio restored_file_size / PAGE_SIZE.
NOTES:
    [23.01.2025] pzotov
    1. One need to be sure that firebird.conf does NOT contain DatabaseAccess = None.
    2. Value of REQUIRED_ALIAS must be EXACTLY the same as alias specified in the pre-created databases.conf
       (for LINUX this equality is case-sensitive, even when aliases are compared!)
    3. Content of databases.conf must be taken from $QA_ROOT/files/qa-databases.conf (one need to replace it before every test session).
       Discussed with pcisar, letters since 30-may-2022 13:48, subject:
       "new qa, core_4964_test.py: strange outcome when use... shutil.copy() // comparing to shutil.copy2()"
    4. Value of DefaultDBCachePages must be LESS than result of division restored_file_size/PAGE_SIZE otherwise fix can not be observed.

    Confirmed problem on 6.0.0.584, 5.0.2.1592 (only when Servermode = 'Super'; SC and CS not affected).
    Checked on 6.0.0.590, 5.0.2.1601 -- all OK; 4.x. seems not affected (checked on 4.0.6.3174-ffd396f, date: 23-dec-2024).
"""

import locale
import re
import os
from pathlib import Path

import pytest
from firebird.qa import *

substitutions = [('[ \t]+', ' '), ]

PAGE_SIZE = 8192
REQUIRED_ALIAS = 'tmp_gh_8391_alias'

db = db_factory(filename = '#' + REQUIRED_ALIAS, do_not_create = True, do_not_drop = True)
act = python_act('db', substitutions = substitutions)

tmp_fbk = temp_file('tmp_gh_8391.restored.fbk')
tmp_log = temp_file('tmp_gh_8391.restored.log')

@pytest.mark.version('>=4.0.6')
def test_1(act: Action, tmp_fbk: Path, tmp_log: Path, capsys):
    
    if act.get_server_architecture() != 'SuperServer':
        pytest.skip('Applies only to SuperServer')

    # Scan line-by-line through databases.conf, find line starting with REQUIRED_ALIAS and extract name of file that
    # must be created in the $(dir_sampleDb)/qa/ folder.
    # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
    p_required_alias_ptn =  re.compile( '^(?!#)((^|\\s+)' + REQUIRED_ALIAS + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
    fname_in_dbconf = None

    with open(act.home_dir/'databases.conf', 'r') as f:
        for line in f:
            if p_required_alias_ptn.search(line):
                # If databases.conf contains line like this:
                #     tmp_8391_alias = $(dir_sampleDb)/qa/tmp_qa_8391.fdb 
                # - then we extract filename: 'tmp_qa_8391.fdb' (see below):
                fname_in_dbconf = Path(line.split('=')[1].strip()).name
                break

    # if 'fname_in_dbconf' remains undefined here then propably REQUIRED_ALIAS not equals to specified in the databases.conf!
    #
    assert fname_in_dbconf

    test_sql = f"""
        set list on;
        create database '{REQUIRED_ALIAS}' user {act.db.user} password '{act.db.password}' page_size {PAGE_SIZE};
        commit;
        recreate sequence g;
        recreate table test(id int, b blob);

        set autoterm on;
        execute block as
            declare n int;
        begin
            insert into test(id, b) values(gen_id(g,1), gen_uuid());
            insert into test(id, b)
            values(
                    gen_id(g,1)
                    ,(select list(gen_uuid()) as s from rdb$types,rdb$types)
                  );

            insert into test(id, b)
            values(
                    gen_id(g,1)
                    ,(select list(gen_uuid()) as s from (select 1 x from rdb$types,rdb$types,rdb$types rows 800000))
                  );
        end
        ;
        commit;
        select mon$database_name, mon$page_buffers from mon$database;
    """

    test_fdb_file = 'UNDEFINED'
    act.isql(switches=['-q'], input = test_sql, connect_db = False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
    for line in act.stdout.splitlines():
        if line.startswith('mon$database_name'.upper()):
            test_fdb_file = line.split()[1]
        if line.startswith('mon$page_buffers'.upper()):
            test_fdb_buffers = int(line.split()[1])

    assert test_fdb_buffers == 128
    act.reset()

    act.gbak(switches=['-b', act.db.dsn, str(tmp_fbk)], combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.clean_stdout == ''
    act.reset()

    act.gbak(switches=['-rep', '-v', '-st', 'w', str(tmp_fbk), REQUIRED_ALIAS], combine_output = True, io_enc = locale.getpreferredencoding())

    # gbak: NNNN total statistics
    gbak_write_total_stat_ptn = re.compile( r'gbak(:)?\s+\d+\s+total\s+statistics', re.IGNORECASE )

    EXPECTED_MSG = 'EXPECTED: gbak total statistics for writes NOT LESS than ratio DB_FILE_SIZE / PAGE_SIZE'
    for line in act.stdout.splitlines():
        if gbak_write_total_stat_ptn.search(line):
            gbak_total_writes = int(line.split()[1])
            restored_file_size = os.stat(test_fdb_file).st_size
            if gbak_total_writes >= restored_file_size / PAGE_SIZE:
                print(EXPECTED_MSG)
            else:
                print(f'UNEXPECTED: {gbak_total_writes=} -- LESS than {restored_file_size/PAGE_SIZE=}')

    act.expected_stdout = EXPECTED_MSG
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
