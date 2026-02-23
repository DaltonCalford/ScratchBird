#coding:utf-8

"""
ID:          issue-6947
ISSUE:       6947
TITLE:       Query to mon$ tables does not return data when the encryption/decryption thread is running
DESCRIPTION:
  Test creates table with wide indexed column and add some data to it.
  Volume of data must be big enough so that the encryption thread will not complete instantly.

  Then 'ALTER DATABASE ENCRYPT...' is issued and we start loop 'select mon$crypt_page from mon$database'.
  This loop must return NOT LESS than <MIN_DISTINCT_ENCRYPTED_PAGES> values greater than zero (i.e. it must
  show progress of this process; zero values mean that encryption either did not start or already finished).
  On each iteration we also check whether encryption completed (use ISQL call with 'show database' command)
  or may be it is too slow and we have to cancel loop because of expiration <MAX_WAITING_ENCR_FINISH> limit.

  When this loop is finished, we check number of elements in the encrypted_pages_set (unique values returned
  by query from mon$database) and show outcome message.

  Confirmed bug on 5.0.0.219 (Windows), 5.0.0.236 (Linux).
  Checked on 5.0.0.240 (Windows; SS/Cs), 5.0.0.241 (Linux; SS/CS).
FBTEST:      bugs.core_6947
NOTES:
    [20.06.2022] pzotov
    Settings for encryption are taken from act.files_dir/'test_config.ini' file.
    Checked on 5.0.0.509 - both Windows and Linux.

    [11.03.2023] pzotov
    Marked as SKIPPED because covered by core_6048_test.
    Probably will be deleted soon.

    [18.01.2025] pzotov
    Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
    in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
    Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
    This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
    The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
"""
import os
import datetime as py_dt
from datetime import timedelta
import time

import pytest
from firebird.qa import *
from firebird.driver import ShutdownMode, ShutdownMethod, DatabaseError

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

FLD_LEN = 500
N_ROWS = 50000
MIN_DISTINCT_ENCRYPTED_PAGES = 3

init_ddl = f"""
    recreate table test(s varchar({FLD_LEN}));
    commit;
    set term ^;
    execute block as
        declare n int = {N_ROWS};
    begin
        while (n>0) do
        begin
            insert into test(s) values(lpad('', {FLD_LEN}, uuid_to_char(gen_uuid())));
            n = n - 1;
        end
    end
    ^
    -- for debug, trace must be started with log_proc = true:
    create procedure sp_tmp(a_page int) as
    begin
        -- nop --
    end
    ^
    set term ;^
    commit;
    create index test_s on test(s);
"""
db = db_factory(init = init_ddl)
act = python_act('db', substitutions=[('[ \t]+', ' ')])

expected_stdout = f"Expected: query to mon$database returned not less then {MIN_DISTINCT_ENCRYPTED_PAGES} encrypted pages."

@pytest.mark.encryption
@pytest.mark.version('>=5.0')
@pytest.mark.skip('Covered by bugs/core_6048_test')

def test_1(act: Action, capsys):

    act.db.set_sync_write() # here we want DB be  encrypted for some *valuable* time

    encryption_started = False
    encryption_finished = False
    encrypted_pages_set = set()

    with act.db.connect() as con:

        t1=py_dt.datetime.now()
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


        cur = con.cursor()
        cu2 = con.cursor()

        ps, rs = None, None
        try:
            ps = cur.prepare('select mon$crypt_page from mon$database')
            while encryption_started:
                t2=py_dt.datetime.now()
                d1=t2-t1
                if d1.seconds*1000 + d1.microseconds//1000 > MAX_WAITING_ENCR_FINISH:
                    print(f'TIMEOUT EXPIRATION: encryption took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {MAX_WAITING_ENCR_FINISH} ms.')
                    break

                # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                # We have to store result of cur.execute(<psInstance>) in order to
                # close it explicitly.
                # Otherwise AV can occur during Python garbage collection and this
                # causes pytest to hang on its final point.
                # Explained by hvlad, email 26.10.24 17:42
                rs = cur.execute(ps)
                for r in rs:
                    p = r[0]

                cu2.callproc('sp_tmp', [ p, ] )
                con.commit()

                if p > 0:
                    encrypted_pages_set.add(p)
                    if len(encrypted_pages_set) >= MIN_DISTINCT_ENCRYPTED_PAGES:
                        # We got enough data from mon$database to conclude that encryption is in PROGRESS.
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

        except DatabaseError as e:
            print( e.__str__() )
            print(e.gds_codes)
        finally:
            if rs:
                rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
            if ps:
                ps.free()

    
    if encryption_started:
        if len(encrypted_pages_set) >= MIN_DISTINCT_ENCRYPTED_PAGES:
            print(expected_stdout)
        elif len(encrypted_pages_set) > 0:
            print(f'PROBABLY ERROR OR TOO SLOW ENCRYPTION: found {len(encrypted_pages_set)} encrypted pages, minimal expected: {MIN_DISTINCT_ENCRYPTED_PAGES}')
        else:
            print(f'ERROR: query to mon$database was blocked during encryption.')
    else:
        print('ERROR: nothing was tested because encryption did not start.')

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
