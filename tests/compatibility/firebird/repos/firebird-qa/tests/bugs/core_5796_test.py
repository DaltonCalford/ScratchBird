#coding:utf-8

"""
ID:          issue-6059
ISSUE:       6059
TITLE:       gstat may produce faulty report about presence of some none-encrypted pages in database
DESCRIPTION:
    Test has been fully re-implemented because its initial version was based on old gstat that did not know '-e' command switch.
    We create two tables and add data into both of them. After this we DROP first table which must lead to deallocating some
    valuable number of pages (of all types: data, index and blobs).
    Also, we create lot of generators in order they occupy more than one generators page.

    Then we run 'ALTER DATABASE ENCRYPT ...' and wait until 'SHOW DATABASE' will display text that database encrypted.
    Finally, we run 'gstat -e ...' and check its output for presence of lines with statistics for encrypred/non-encrypted pages.
    Every such line must finish with '0' (this is number of NON-crypted pages).

JIRA:        CORE-5796
FBTEST:      bugs.core_5796
NOTES:
    [12.06.2022] pzotov
    Checked on 4.0.1.2692, 3.0.8.33535 - both on Linux and Windows.
"""
import os
import re
import time
import datetime as py_dt
from datetime import timedelta

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

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


N_ROWS = 25
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
    begin
        while (n > 0) do
        begin
            execute statement 'create sequence gen_' || n;
            n = n - 1;
        end
    end ^
    set term ;^
    commit;
"""

db = db_factory(init = init_script)
act = python_act('db')

@pytest.mark.encryption
@pytest.mark.version('>=3.0.4')
def test_1(act: Action, capsys):

    with act.db.connect() as con:

        t1=py_dt.datetime.now()
        d1 = t1-t1
        sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}" key "{ENCRYPTION_KEY}"'
        try:
            con.execute_immediate(sttm)
            con.commit()
        except DatabaseError as e:
            print( e.__str__() )

        act.expected_stdout = ''
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

        while True:
            t2=py_dt.datetime.now()
            d1=t2-t1
            if d1.seconds*1000 + d1.microseconds//1000 > MAX_WAITING_ENCR_FINISH:
                con.execute_immediate(f"select 'TIMEOUT EXPIRATION: encryption took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {MAX_WAITING_ENCR_FINISH} ms.' as msg from rdb$database")
                break

            # Possible output:
            #     Database not encrypted
            #     Database encrypted, crypt thread not complete
            act.isql(switches=['-q'], input = 'show database;', combine_output = True)
            if 'Database encrypted' in act.stdout:
                if 'not complete' in act.stdout:
                    pass
                else:
                    break
            act.reset()

        if d1.seconds*1000 + d1.microseconds//1000 <= MAX_WAITING_ENCR_FINISH:
            act.reset()
            act.gstat(switches=['-e'])

            # Data pages: total 884803, encrypted 884803, non-crypted 0
            # ...
            pattern = re.compile('(data|index|blob|generator)\\s+pages[:]{0,1}\\s+total[:]{0,1}\\s+\\d+[,]{0,1}\\s+encrypted[:]{0,1}\\s+\\d+.*[,]{0,1}non-crypted[:]{0,1}\\s+\\d+.*', re.IGNORECASE)
            for line in act.stdout.splitlines():
                if pattern.match(line.strip()):
                    # We assume that every line finishes with number of NON-crypted pages, and this number must be 0:
                    words = line.split()
                    if words[-1] == '0':
                        print(words[0] + ': expected, ' +  words[-1])
                    else:
                        print(words[0] + ': UNEXPECTED, ' +  words[-1])

        else:
            print(f'TIMEOUT EXPIRATION: encryption took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {MAX_WAITING_ENCR_FINISH} ms.')

        act.expected_stdout = """
            Data: expected, 0
            Index: expected, 0
            Blob: expected, 0
            Generator: expected, 0
        """

        act.stdout = capsys.readouterr().out

        assert act.clean_stdout == act.clean_expected_stdout
