#coding:utf-8

"""
ID:          issue-6293
ISSUE:       6293
TITLE:       GTTs do not release used space
DESCRIPTION:
    1) runs loop for two iterations (see 'for ClrRetainGTT in (0,1): ...')
    2) on each iteration:
       2.1)  makes copy of test DB to the file which is specified in databases.conf as 
             database for alias with name = 'tmp_core_6043_gtt_0_alias', 'tmp_core_6043_gtt_1_alias'.
             NOTE: it is supposed that we have pre-created databases.conf with these TWO ALIASES,
             and each of them have in its details parameter ClearGTTAtRetaining = 0 and 1 respectively.
       2.2) launches ISQL which must connect to appropriate alias and execute script from ticket;
    3) checks that:
      3.1) for ClearGTTAtRetaining = 0 (value for backward compatibility):
          * COMMIT RETAIN preserves record that was inserted in the statement before this commit;
          * ROLLBACK RETAIN does NOT delete record that was inserted before COMMIT RETAIN.
      3.2) for ClearGTTAtRetaining = 1 (default value) NO records will be in the GTT after commit/rollback RETAIN.
JIRA:        CORE-6043
FBTEST:      bugs.core_6043
NOTES:
    [06.08.2022] pzotov
    Confirmed problem on 3.0.5.33115 (snapshot date: 26-mar-2019):
    records for commit/rollback retain were NOT preserved despite setting ClearGTTAtRetaining = 0.
    Previous checks:
        4.0.0.1687 SS: 1.536s.
        4.0.0.1685 CS: 2.026s.
        3.0.5.33207 SS: 1.435s.
        3.0.5.33152 SC: 1.243s.
        3.0.5.33206 CS: 2.626s.
    Checked on 5.0.0.591, 4.0.1.2692, 3.0.8.33535 - both on Windows and Linux.

    [01.12.2023] pzotov
    Adjusted pytest.mark.version: test can run only on FB 3.x and 4.x.
    Config parameter ClearGTTAtRetaining was removed from FB 5.x+
    (see "Task #7897 : Remove obsolete setting ClearGTTAtRetaining").
"""

import re
from pathlib import Path

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions=[('[ \t]+', ' ')])

@pytest.mark.version('>=3.0.5','<5.0')
def test_1(act: Action, capsys):
    
    for ClrRetainGTT in (0,1):
        chk_alias = f'tmp_core_6043_gtt_{ClrRetainGTT}_alias' # 'tmp_core_6043_gtt_0_alias', 'tmp_core_6043_gtt_1_alias'

        #for chk_alias in REQUIRED_ALIASES:
        # Scan line-by-line through databases.conf, find line starting with <chk_alias> and extract name of file that
        # must be created in the $(dir_sampleDb)/qa/ folder. This name will be used further as target database (tmp_fdb).
        # NOTE: we have to SKIP lines which are commented out, i.e. if they starts with '#':
        ptn_checked_alias =  re.compile( '^(?!#)((^|\\s+)' + chk_alias + ')\\s*=\\s*\\$\\(dir_sampleDb\\)/qa/', re.IGNORECASE )
        fname_in_dbconf = None

        with open(act.home_dir/'databases.conf', 'r') as f:
            for line in f:
                if ptn_checked_alias.search(line):
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
        
        # Permiss. Error: [Errno 13] Permiss. denied --> probably because
        # Firebird was started by root rather than current (non-privileged) user.
        #
        tmp_fdb.write_bytes(act.db.db_path.read_bytes())


        check_sql = f'''
            set bail on;
            set list on;
            connect '{act.host+":" if act.host else ""}{chk_alias}' user {act.db.user} password '{act.db.password}';
                 
            select '{ClrRetainGTT}' as "ClearGTTAtRetaining:" from rdb$database;

            recreate global temporary table gtt (id int) on commit delete rows;
            commit;

            set count off;
            insert into gtt values (3);
            commit retain;

            set count on;
            select * from gtt; -- point 1

            set count off;
            insert into gtt values (4);
            rollback retain;

            set count on;
            select * from gtt; -- point 2
        '''

        if ClrRetainGTT == 0:
            act.expected_stdout = f"""
                ClearGTTAtRetaining: {ClrRetainGTT}
                ID 3
                Records affected: 1
                ID 3
                Records affected: 1
            """
        else:
            act.expected_stdout = f"""
                ClearGTTAtRetaining: {ClrRetainGTT}
                Records affected: 0
                Records affected: 0
            """

        try:
            act.isql(switches = ['-q'], input = check_sql, connect_db=False, credentials = False, combine_output = True)
        finally:
            tmp_fdb.unlink()

        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
