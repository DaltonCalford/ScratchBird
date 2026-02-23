#coding:utf-8

"""
ID:          issue-6070
ISSUE:       6070
TITLE:       Support backup of encrypted databases
DESCRIPTION:

    Test creates two tables, adds data, and drops one of them (thus there are deallocated pages in the database).
    Then it creates lot of sequences with different initial values and increments.
    Database is encrypted after this step.

    After this, we get initial content of: firebird.log, metadata('isql -x') and sequences value and store them 
    in variables fblog_1, meta_1 and sequ_1.
    Then we run backup and restore, and validate database.
    Further, we extract again metadata and sequences value of restored DB, and store them in fblog_2, meta_2 and sequ_2.

    Difference between fblog_1 and fblog_2 must look like this: "Validation finished: 0 errors, 0 warnings, 0 fixed".
    NO difference must be between meta_1 vs meta_2, and sequ_1 vs sequ_2

JIRA:        CORE-5808
FBTEST:      bugs.core_5808
NOTES:
    [12.06.2022] pzotov
    Checked on 4.0.1.2692 - both on Linux and Windows.
    NB: duration on Linux ~40 s; on Windows ~22 s.
"""
import os
import re
import time
import datetime as py_dt
from datetime import timedelta
from io import BytesIO
from difflib import unified_diff

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError
from firebird.driver import SrvRestoreFlag, SrvRepairFlag

###########################
###   S E T T I N G S   ###
###########################

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
enc_settings = QA_GLOBALS['encryption']

# ACHTUNG: this must be carefully tuned on every new host:
#
MAX_WAITING_ENCR_FINISH = int(enc_settings['MAX_WAIT_FOR_ENCR_FINISH_WIN' if os.name == 'nt' else 'MAX_WAIT_FOR_ENCR_FINISH_NIX'])
assert MAX_WAITING_ENCR_FINISH > 0

ENCRYPTION_PLUGIN = enc_settings['encryption_plugin'] # fbSampleDbCrypt
ENCRYPTION_KEY = enc_settings['encryption_key'] # Red


N_ROWS = 15
init_script = f"""
    create table test1(s varchar(784) unique, b blob);
    create table test2(s varchar(784) unique, b blob);
    commit;
    insert into test1(s) select lpad('',784,uuid_to_char(gen_uuid())) from (select 1 i from rdb$types rows {N_ROWS}),(select 1 i from rdb$types rows {N_ROWS});
    insert into test2(s) select lpad('',784,uuid_to_char(gen_uuid())) from (select 1 i from rdb$types rows {N_ROWS});
    update test1 set b = (select list( (select gen_uuid() from rdb$database) ) from (select 1 i from rdb$types rows {N_ROWS}),(select 1 i from rdb$types rows {N_ROWS}));
    update test2 set b = (select list( (select gen_uuid() from rdb$database) ) from (select 1 i from rdb$types rows {N_ROWS}),(select 1 i from rdb$types rows {N_ROWS}));
    commit;
    drop table test1;
    commit;
    set term ^;
    execute block as
        declare n int = 16000;
        declare i bigint = -2147483648;
        declare b bigint = 9223372036854775807;
    begin
        while (n > 0) do
        begin
            execute statement 'create sequence gen_' || n || ' start with ' || (b-abs(i)) || ' increment by ' || (i+1) ;
            n = n - 1;
            i = i + 1;
        end
    end ^
    set term ;^
    commit;
"""

db = db_factory(init = init_script)

act = python_act('db')

@pytest.mark.encryption
@pytest.mark.version('>=4.0')
def test_1(act: Action, capsys):

    encryption_started = False
    encryption_finished = False
    with act.db.connect() as con:

        t1=py_dt.datetime.now()
        d1 = t1-t1
        sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}" key "{ENCRYPTION_KEY}"'
        try:
            con.execute_immediate(sttm)
            con.commit()
            encryption_started = True
        except DatabaseError as e:
            # -ALTER DATABASE failed
            # -Crypt plugin fbSampleDbCrypt failed to load
            #  ==> no sense to do anything else, encryption_started remains False.
            print( e.__str__() )

        while encryption_started:
            t2=py_dt.datetime.now()
            d1=t2-t1
            if d1.seconds*1000 + d1.microseconds//1000 > MAX_WAITING_ENCR_FINISH:
                print(f'TIMEOUT EXPIRATION: encryption took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {MAX_WAITING_ENCR_FINISH} ms.')
                break

            # Possible output:
            #     Database not encrypted
            #     Database encrypted, crypt thread not complete
            act.isql(switches=['-q'], input = 'show database;', combine_output = True)
            if 'Database encrypted' in act.stdout:
                if 'not complete' in act.stdout:
                    pass
                else:
                    encryption_finished = True
                    break
            act.reset()


        act.expected_stdout = ''
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
    

    if encryption_finished:
        # Extract metadata from initial DB
        act.isql(switches=['-x'])
        meta_1 = act.stdout
        act.reset()

        act.isql(switches=['-q'], input = 'show sequ;', combine_output = True)
        sequ_1 = act.stdout
        act.reset()

        # Snippet from core-1725:
        backup = BytesIO()
        with act.connect_server() as srv:
            
            srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
            backup.seek(0)
            srv.database.local_restore(backup_stream=backup, database=act.db.db_path, flags = SrvRestoreFlag.REPLACE)
     
            # Get FB log before validation, run validation and get FB log after it:
            fblog_1 = act.get_firebird_log()
            srv.database.repair(database=act.db.db_path, flags=SrvRepairFlag.CORRUPTION_CHECK)
            fblog_2 = act.get_firebird_log()

        # Extract metadata from restored DB
        act.isql(switches=['-x'])
        meta_2 = act.stdout
        act.reset()

        act.isql(switches=['-q'], input = 'show sequ;', combine_output = True)
        sequ_2 = act.stdout
        act.reset()

        diff_meta = ''.join(unified_diff(meta_1.splitlines(), meta_2.splitlines()))

        p_diff = re.compile('Validation finished: \\d+ errors, \\d+ warnings, \\d+ fixed')
        validation_result = ''
        for line in unified_diff(fblog_1, fblog_2):
            if line.startswith('+') and p_diff.search(line):
                validation_result =line.strip().replace('\t', ' ')
                break


        assert diff_meta == ''
        assert validation_result == '+ Validation finished: 0 errors, 0 warnings, 0 fixed'
        assert sequ_1 == sequ_2
