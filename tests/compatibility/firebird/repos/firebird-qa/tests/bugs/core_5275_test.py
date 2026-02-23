#coding:utf-8

"""
ID:          issue-5553
ISSUE:       5553
TITLE:       Expression index may become inconsistent if CREATE INDEX was interrupted after b-tree creation but before commiting
DESCRIPTION:
  This test (and CORE- ticket) has been created after wrong initial implementation of test for CORE-1746.
  Scenario:
  1. ISQL_1 is launched as child async. process, inserts 1000 rows and then falls in pause (delay) ~10 seconds;
  2. ISQL_2 is launched as child async. process in Tx = WAIT, tries to create index on the table which is handled
    by ISQL_1 and immediatelly falls in pause because of waiting for table lock.
  3. ISQL_3 is launched in SYNC mode and does 'DELETE FROM MON$ATTACHMENTS' thus forcing other attachments to be
    closed with raising 00803/connection shutdown.
  4. Repeat step 1. On WI-T4.0.0.258 this step lead to:
    "invalid SEND request (167), file: JrdStatement.cpp line: 325", 100% reproducilbe.

  Beside above mentioned steps, we also:
  1) compare content of old/new firebird.log (difference): it should NOT contain line "consistency check";
  2) run database online validation: it should NOT report any error in the database.
JIRA:        CORE-5275
FBTEST:      bugs.core_5275
NOTES:
  [18.08.2020] pzotov
      FB 4.x has incompatible behaviour with all previous versions since build 4.0.0.2131 (06-aug-2020):
      statement 'alter sequence <seq_name> restart with 0' changes rdb$generators.rdb$initial_value to -1 thus next call
      gen_id(<seq_name>,1) will return 0 (ZERO!) rather than 1.
      See also CORE-6084 and its fix: https://github.com/FirebirdSQL/firebird/commit/23dc0c6297825b2e9006f4d5a2c488702091033d
      This is considered as *expected* and is noted in doc/README.incompatibilities.3to4.txt
      Because of this, it was decided to replace 'alter sequence restart...' with subtraction of two gen values:
      c = gen_id(<g>, -gen_id(<g>, 0)) -- see procedure sp_restart_sequences.
  [15.09.2022] pzotov
      Fixed 18.06.2016 15:57
      Checked on Linux and Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.591
  [16.03.2023] pzotov
      Reduced timeouts according to values that were used in old test that could reproduce problem on WI-T4.0.0.258
      Check again reproducing of problem: confirmed for WI-4.0.0.258 Classic, got:
      INTERNAL FIREBIRD CONSISTENCY CHECK (INVALID SEND REQUEST (167), FILE: JRDSTATEMENT.CPP LINE: 325)
      Confirmed fix on WI-4.0.0.262 (currently checked only for Windows).
  [12.04.2023] pzotov
      This test FAILS on Linux when running against FB 4.x (almost every run on Classic, but also it can fail on Super).
      Connection that is waiting for COMMIT during index creation for some reason can finish its work successfully,
      despite the fact that we issue 'delete from mon$attachments' and all transactions have to be rolled back.
      Issue that was described in the ticket can be reproduced if attachment will be killed during creation of SECOND
      (non-computed) index for big table within the same transaction that creates first (computed-by) index.
      Perhaps, one need to query IndexRoot Page in some other ('monitoring') connection and run 'delete from mon$attachments'
      command exactly at the moment when result of parsing shows that we have only 1st index for selected relation.
      Discussed with dimitr et al, letters ~20-mar-2023.
      Test needs to be fully re-implemented, but it remains avaliable for Windows because it shows stable results there.

    [01.07.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' and variables - to be substituted in expected_* on FB 6.x
    Separated expected output for FB major versions prior/since 6.x.
    
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import platform
import subprocess
import time
from pathlib import Path
from difflib import unified_diff
import re

import pytest
from firebird.qa import *

substitutions = [('0: CREATE INDEX LOG: RDB_EXPR_BLOB.*', '0: CREATE INDEX LOG: RDB_EXPR_BLOB'),
                 ('BULK_INSERT_START.*', 'BULK_INSERT_START'),
                 ('.*KILLED BY DATABASE ADMINISTRATOR.*', ''),
                 ('BULK_INSERT_FINISH.*', 'BULK_INSERT_FINISH'),
                 ('CREATE_INDX_START.*', 'CREATE_INDX_START'),
                 ('AFTER LINE.*', 'AFTER LINE'), ('RECORDS AFFECTED:.*', 'RECORDS AFFECTED:'),
                 ('[0-9][0-9]:[0-9][0-9]:[0-9][0-9].[0-9][0-9]', ''),
                 ('RELATION [0-9]{3,4}', 'RELATION')]

init_script = """
    create or alter procedure sp_ins(n int) as begin end;

    recreate table test(x int unique using index test_x, s varchar(10) default 'qwerty' );

    create sequence g;
    commit;

    set term ^;
    create or alter procedure sp_ins(n int) as
    begin
        while (n>0) do
        begin
            insert into test(x) values(gen_id(g,1));
            n = n - 1;
        end
    end
    ^
    set term ;^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=substitutions)

bulk_insert_script = temp_file('bulk_insert.sql')
bulk_insert_output = temp_file('bulk_insert.out')
create_idx_script = temp_file('create_idx.sql')
create_idx_output = temp_file('create_idx.out')

ROWS_TO_ADD = 1000

bulk_insert = f"""
    set bail on;
    set list on;

    -- DISABLED 19.08.2020: alter sequence g restart with 0;

    delete from test;
    commit;
    set transaction lock timeout 10; -- THIS LOCK TIMEOUT SERVES ONLY FOR DELAY, see below auton Tx start.

    select current_timestamp as bulk_insert_start from rdb$database;
    set term ^;
    execute block as
        declare i int;
    begin
        i = gen_id(g, -gen_id(g, 0)); -- restart sequence, since 19.08.2020
        execute procedure sp_ins({ROWS_TO_ADD});
        begin
            -- #########################################################
            -- #######################  D E L A Y  #####################
            -- #########################################################
            in autonomous transaction do
            insert into test( x ) values({ROWS_TO_ADD}); -- this will cause delay because of duplicate in index
        when any do
            begin
                i  =  gen_id(g,1);
            end
        end
    end
    ^
    set term ;^
    commit;
    select current_timestamp as bulk_insert_finish from rdb$database;
"""

create_idx = f"""
    set bail on;
    set list on;
    set blob all;
    select
        iif( gen_id(g,0) > 0 and gen_id(g,0) < 1 + {ROWS_TO_ADD},
             'OK, IS RUNNING',
             iif( gen_id(g,0) <=0,
                  'WRONG: not yet started, current gen_id='||gen_id(g,0),
                  'WRONG: already finished, rows_to_add='||{ROWS_TO_ADD} ||', current gen_id='||gen_id(g,0)
                )
           ) as inserts_state,
        current_timestamp as create_indx_start
    from rdb$database;
    set autoddl off;
    commit;

    set echo on;
    set transaction WAIT;

    create index test_WAIT on test computed by('WAIT' || s);
    set echo off;
    commit;

    select
        iif(  gen_id(g,0) >= 1 + {ROWS_TO_ADD},
              'OK, FINISHED',
              'SOMETHING WRONG: current gen_id=' || gen_id(g,0)||', rows_to_add='||{ROWS_TO_ADD}
           ) as inserts_state
    from rdb$database;

    set count on;
    select
        rdb$index_name
        ,coalesce(rdb$unique_flag,0) as rdb$unique_flag
        ,coalesce(rdb$index_inactive,0) as rdb$index_inactive
        ,rdb$expression_source as rdb_expr_blob
    from rdb$indices ri
    where ri.rdb$index_name = upper('test_WAIT')
    ;
    set count off;
    set echo on;
    set plan on;
    select 1 from test where 'WAIT' || s > '' rows 0;
    set plan off;
    set echo off;
    commit;
    drop index test_WAIT;
    commit;
"""

kill_att = """
    set count on;
    set list on;
    commit;
    delete from mon$attachments where mon$attachment_id<>current_connection;
"""


def print_validation(line: str) -> None:
    if line.strip():
        print(f'VALIDATION STDOUT: {line.upper()}')

@pytest.mark.version('>=3.0')
@pytest.mark.skipif(platform.system() != 'Windows', reason='UNSTABLE on Linux FB 4.x (mostly on Classic). See notes.')

def test_1(act: Action, bulk_insert_script: Path, bulk_insert_output: Path,
           create_idx_script: Path, create_idx_output: Path, capsys):

    bulk_insert_script.write_text(bulk_insert)
    create_idx_script.write_text(create_idx)
    # Get Firebird log before test
    log_before = act.get_firebird_log()
    #
    for step in range(2):
        # Start bulk insert
        with open(bulk_insert_output, mode='w') as bulk_insert_out, \
             open(create_idx_output, mode='w') as create_idx_out:
            p_bulk_insert = subprocess.Popen([act.vars['isql'], '-q',
                                              '-i', str(bulk_insert_script),
                                              '-user', act.db.user,
                                              '-password', act.db.password, act.db.dsn],
                                             stdout=bulk_insert_out, stderr=subprocess.STDOUT)
            time.sleep(2)

            # Create index
            p_create_idx = subprocess.Popen([act.vars['isql'], '-q',
                                             '-i', str(create_idx_script),
                                             '-user', act.db.user,
                                             '-password', act.db.password, act.db.dsn],
                                            stdout=create_idx_out, stderr=subprocess.STDOUT)
            time.sleep(2)

            # kill isql connections
            act.isql(switches=[], input=kill_att)

            p_bulk_insert.wait()
            p_create_idx.wait()

        # Print logs
        for line in act.clean_string(bulk_insert_output.read_text()).splitlines():
            if line:
                print(f'{step}: BULK INSERTS LOG: {line.strip().upper()}')
        for line in create_idx_output.read_text().splitlines():
            if line:
                print(f'{step}: CREATE INDEX LOG: {line.strip().upper()}')
        for line in act.clean_string(act.stdout).splitlines():
            if line:
                print(f'{step}: KILL ATTACH LOG: {line.strip().upper()}')

    # Run database validation
    with act.connect_server() as srv:
        srv.database.validate(database=act.db.db_path, callback=print_validation)

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  '"PUBLIC".'
    TABLE_NAME = 'TEST' if act.is_version('<6') else  '"TEST"'
    INDEX_NAME = 'TEST_X' if act.is_version('<6') else  '"TEST_X"'

    expected_stdout = f"""
        0: BULK INSERTS LOG: BULK_INSERT_START
        0: BULK INSERTS LOG: STATEMENT FAILED, SQLSTATE = 08003
        0: BULK INSERTS LOG: CONNECTION SHUTDOWN
        0: BULK INSERTS LOG: AFTER LINE
        0: CREATE INDEX LOG: INSERTS_STATE                   OK, IS RUNNING
        0: CREATE INDEX LOG: CREATE_INDX_START
        0: CREATE INDEX LOG: SET TRANSACTION WAIT;
        0: CREATE INDEX LOG: CREATE INDEX TEST_WAIT ON TEST COMPUTED BY('WAIT' || S);
        0: CREATE INDEX LOG: SET ECHO OFF;
        0: CREATE INDEX LOG: STATEMENT FAILED, SQLSTATE = 08003
        0: CREATE INDEX LOG: CONNECTION SHUTDOWN
        0: CREATE INDEX LOG: AFTER LINE
        0: KILL ATTACH LOG: RECORDS AFFECTED:
        1: BULK INSERTS LOG: BULK_INSERT_START
        1: BULK INSERTS LOG: STATEMENT FAILED, SQLSTATE = 08003
        1: BULK INSERTS LOG: CONNECTION SHUTDOWN
        1: BULK INSERTS LOG: AFTER LINE
        1: CREATE INDEX LOG: INSERTS_STATE                   OK, IS RUNNING
        1: CREATE INDEX LOG: CREATE_INDX_START
        1: CREATE INDEX LOG: SET TRANSACTION WAIT;
        1: CREATE INDEX LOG: CREATE INDEX TEST_WAIT ON TEST COMPUTED BY('WAIT' || S);
        1: CREATE INDEX LOG: SET ECHO OFF;
        1: CREATE INDEX LOG: STATEMENT FAILED, SQLSTATE = 08003
        1: CREATE INDEX LOG: CONNECTION SHUTDOWN
        1: CREATE INDEX LOG: AFTER LINE
        1: KILL ATTACH LOG: RECORDS AFFECTED:
        VALIDATION STDOUT: 20:05:26.86 VALIDATION STARTED
        VALIDATION STDOUT: 20:05:26.86 RELATION 128 ({SQL_SCHEMA_PREFIX}{TABLE_NAME})
        VALIDATION STDOUT: 20:05:26.86   PROCESS POINTER PAGE    0 OF    1
        VALIDATION STDOUT: 20:05:26.86 INDEX 1 ({SQL_SCHEMA_PREFIX}{INDEX_NAME})
        VALIDATION STDOUT: 20:05:26.86 RELATION 128 ({SQL_SCHEMA_PREFIX}{TABLE_NAME}) IS OK
        VALIDATION STDOUT: 20:05:26.86 VALIDATION FINISHED
    """
    
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()


    # Get Firebird log after test
    log_after = act.get_firebird_log()

    allowed_patterns = [re.compile('consistency\\s+check',re.IGNORECASE),
                        re.compile('terminate\\S+ abnormally',re.IGNORECASE),
                        re.compile('Error\\s+(reading|writing)\\s+data',re.IGNORECASE)
                        ]
    for line in unified_diff(log_before, log_after):
        # ::: NB :::
        # filter(None, [p.search(line) for p in allowed_patterns]) will be None only in Python 2.7.x!
        # In Python 3.x this will retrun "<filter object at 0xNNNNN>" ==> we must NOT use this statement!
        if line.startswith('+') and act.match_any(line, allowed_patterns):
            print(f'Problematic message in firebird.log: {line.upper()}\n')

    assert '' == capsys.readouterr().out

