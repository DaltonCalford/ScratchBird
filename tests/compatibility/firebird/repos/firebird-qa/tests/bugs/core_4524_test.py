#coding:utf-8

"""
ID:          issue-4842
ISSUE:       4842
TITLE:       New gbak option to enable encryption during restore
DESCRIPTION:
    Part of this test was copied from core_6071.fbt.

    We create several generators in the test DB and get number of generators page using query to RDB$PAGES (page_type = 9).
    Also we get page_size and using these data we can obtain binary content of generatord page. 
    We check that for *all* created sequences we can obtain their names and values if read DB file as binary file
    (it must be possible until DB will not encrypted).

    Then we encrypt DB and wait until encryption process will complete.

    After this we:
    * Make backup of this temp DB, using gbak utility and '-KEYHOLDER <name_of_key_holder>' command switch.
    * Make restore from just created backup.
    * Make validation of just restored database by issuing command "gfix -v -full ..."
      (i.e. validate both data and metadata rather than online val which can check user data only).
    * Open restored DB as binary file and attempt to read again generators names - this must fail, their names must be encrypted.
    * Make connect to this DB and check that command 'SHOW SEQU' show all generatord and their values.
JIRA:        CORE-4524
FBTEST:      bugs.core_4524
NOTES:
    [21.09.2022] pzotov
    Test reads settings that are COMMON for all encryption-related tests and stored in act.files_dir/test_config.ini.
    QA-plugin prepares this by defining dictionary with name QA_GLOBALS which reads settings via ConfigParser mechanism.
    Checked on Linux and Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS)

    [29.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""
import os
import binascii
import re
import locale
import datetime as py_dt
import time
from pathlib import Path
from firebird.driver import DatabaseError

import pytest
from firebird.qa import *

db_to_be_encrypted = db_factory()
db_encrypt_restore = db_factory(filename = 'tmp_core_4524.restored.fdb')
tmp_fbk = temp_file('tmp_core_4524.encrypted.fbk')

act_src = python_act('db_to_be_encrypted')
act_res = python_act('db_encrypt_restore')

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act_src.files_dir/'test_config.ini':
enc_settings = QA_GLOBALS['encryption']

# ACHTUNG: this must be carefully tuned on every new host:
#
MAX_WAITING_ENCR_FINISH = int(enc_settings['MAX_WAIT_FOR_ENCR_FINISH_WIN' if os.name == 'nt' else 'MAX_WAIT_FOR_ENCR_FINISH_NIX'])
assert MAX_WAITING_ENCR_FINISH > 0

ENCRYPTION_PLUGIN = enc_settings['encryption_plugin'] # fbSampleDbCrypt
ENCRYPTION_HOLDER = enc_settings['encryption_holder'] # fbSampleKeyHolder
ENCRYPTION_KEY = enc_settings['encryption_key'] # Red

SUCCESS_MSG = 'All sequences FOUND.'

def check_page_for_readable_values(dbname, gen_page_number, pg_size, check_sequence_values, is_encrypted, msg_prefix = ''):

    db_handle = open( dbname, "rb")
    db_handle.seek( gen_page_number * pg_size )
    page_content = db_handle.read( pg_size )
    db_handle.close()
    page_as_hex=binascii.hexlify( page_content )

    # Iterate for each sequence value:
    not_found_lst = []
    any_found_lst = []
    for n in check_sequence_values:

        # Get HEX representation of digital value.
        # NOTE: format( 830624, 'x') is 'caca0' contains five (odd number!) characters.
        hex_string = format(abs(n),'x')

        # Here we 'pad' hex representation to EVEN number of digits in it,
        # otherwise binascii.hexlify fails with "Odd-length string error":
        hex_string = ''.join( ('0' * ( len(hex_string) % 2 ), hex_string ) )

        # ::: NOTE :::
        # Generator value is stored in REVERSED bytes order.
        # dec 830624 --> hex 0x0caca0 --> 0c|ac|a0 --> stored in page as three bytes: {a0; ac; 0c}

        # Decode string that is stored in variable 'hex_string' to HEX number,
        # REVERSE its bytes and convert it to string again for further search
        # in page content:
        #n_as_reversed_hex = binascii.hexlify( hex_string.decode('hex')[::-1] )
        n_as_reversed_hex = binascii.hexlify( bytes.fromhex(hex_string)[::-1] )
        if not n_as_reversed_hex in page_as_hex:
            not_found_lst.append([n, n_as_reversed_hex])
        else:
            any_found_lst.append([n, n_as_reversed_hex])

    if (not is_encrypted) and len(not_found_lst) == 0 or is_encrypted and len(any_found_lst) == 0:
        print(msg_prefix + SUCCESS_MSG)
    else:
        if not is_encrypted:
            print(msg_prefix + 'UNEXPECTEDLY NOT found sequences:')
            for p in not_found_lst:
                print(p)
        if is_encrypted:
            print(msg_prefix + 'UNEXPECTEDLY FOUND sequences:')
            for p in any_found_lst:
                print(p)

#----------------------------------------------------------------------------------------------

@pytest.mark.encryption
@pytest.mark.version('>=4.0')
def test_1(act_src: Action, act_res: Action, tmp_fbk:Path, capsys):

    init_sql = """
        set bail on;
        create sequence gen_ba0bab start with 12192683;
        create sequence gen_badf00d start with 195948557;
        create sequence gen_caca0  start with 830624;
        create sequence gen_c0ffee start with 12648430;
        create sequence gen_dec0de start with 14598366;
        create sequence gen_decade start with 14600926;
        create sequence gen_7FFFFFFF start with 2147483647;
        commit;
    """

    act_src.expected_stdout = ''
    act_src.isql(switches = ['-q'], input = init_sql, combine_output = True, io_enc = locale.getpreferredencoding())
    assert act_src.clean_stdout == act_src.clean_expected_stdout
    act_src.reset()

    check_sequence_values=[]
    with act_src.db.connect() as con:
        with con.cursor() as cur:
            get_current_seq_values='''
                execute block returns( gen_curr bigint) as
                    declare gen_name rdb$generator_name;
                begin
                    for
                        select rdb$generator_name from rdb$generators where rdb$system_flag is distinct from 1 order by rdb$generator_id
                        into gen_name
                    do begin
                        execute statement 'execute block returns(g bigint) as begin g = gen_id('|| gen_name ||', 0); suspend;  end' into gen_curr;
                        suspend;
                    end
                end
            '''

            # Obtain current values of user generators:
            # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            cur.execute(get_current_seq_values)
            for r in cur:
                check_sequence_values += r[0],


            # Obtain page size and ID of generators page:
            # ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            cur.execute('select m.mon$page_size,min(rdb$page_number) from mon$database m cross join rdb$pages p where p.rdb$page_type = 9 group by 1')
            pg_size, gen_page_number = -1,-1
            for r in cur:
                pg_size=r[0]
                gen_page_number=r[1]
                # print(r[0],r[1])


        # Read gen page, convert it to hex and check whether generator values can be found there or no:
        # Expected result: YES for all values because DB not encrypted now.
        # ~~~~~~~~~~~~~~~
        check_page_for_readable_values(act_src.db.db_path, gen_page_number, pg_size, check_sequence_values, False, 'INIT. ')

        act_src.expected_stdout = 'INIT. ' + SUCCESS_MSG
        act_src.stdout = capsys.readouterr().out
        assert act_src.clean_stdout == act_src.clean_expected_stdout
        act_src.reset()

        ######################################################

        t1=py_dt.datetime.now()
        d1 = t1-t1
        sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}" key "{ENCRYPTION_KEY}"'
        try:
            con.execute_immediate(sttm)
            con.commit()
        except DatabaseError as e:
            print( e.__str__() )

        act_src.expected_stdout = ''
        act_src.stdout = capsys.readouterr().out
        assert act_src.clean_stdout == act_src.clean_expected_stdout
        act_src.reset()

        while True:
            t2=py_dt.datetime.now()
            d1=t2-t1
            if d1.seconds*1000 + d1.microseconds//1000 > MAX_WAITING_ENCR_FINISH:
                con.execute_immediate(f"select 'TIMEOUT EXPIRATION: encryption took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {MAX_WAITING_ENCR_FINISH} ms.' as msg from rdb$database")
                break

            # Possible output:
            #     Database not encrypted
            #     Database encrypted, crypt thread not complete
            act_src.isql(switches=['-q'], input = 'show database;', combine_output = True)
            if 'Database encrypted' in act_src.stdout:
                if 'not complete' in act_src.stdout:
                    pass
                else:
                    break
            act_src.reset()

        if d1.seconds*1000 + d1.microseconds//1000 <= MAX_WAITING_ENCR_FINISH:
            act_src.reset()
            act_src.gstat(switches=['-e'])

            # Data pages: total 884803, encrypted 884803, non-crypted 0
            # ...
            pattern = re.compile('(data|index|blob|generator)\\s+pages[:]{0,1}\\s+total[:]{0,1}\\s+\\d+[,]{0,1}\\s+encrypted[:]{0,1}\\s+\\d+.*[,]{0,1}non-crypted[:]{0,1}\\s+\\d+.*', re.IGNORECASE)
            for line in act_src.stdout.splitlines():
                if pattern.match(line.strip()):
                    # We assume that every line finishes with number of NON-crypted pages, and this number must be 0:
                    words = line.split()
                    if words[-1] == '0':
                        print(words[0] + ': expected, ' +  words[-1])
                    else:
                        print(words[0] + ': UNEXPECTED, ' +  words[-1])

            expected_gstat_tail = """
                Data: expected, 0
                Index: expected, 0
                Blob: expected, 0
                Generator: expected, 0
            """
            act_src.expected_stdout = expected_gstat_tail
            act_src.stdout = capsys.readouterr().out
            assert act_src.clean_stdout == act_src.clean_expected_stdout
            act_src.reset()

        else:
            print(f'TIMEOUT EXPIRATION: encryption took {d1.seconds*1000 + d1.microseconds//1000} ms which exceeds limit = {MAX_WAITING_ENCR_FINISH} ms.')

        ######################################################

        # see also core_6071_test.py:
        act_src.gbak(switches=['-b', '-KEYHOLDER', ENCRYPTION_HOLDER, '-crypt', ENCRYPTION_PLUGIN, act_src.db.dsn, str(tmp_fbk)])
        act_src.reset()

        act_src.gbak(switches=['-rep', '-KEYHOLDER', ENCRYPTION_HOLDER, str(tmp_fbk), act_res.db.dsn ])
        act_src.reset()

        act_src.gfix(switches=['-v', '-full', str(act_res.db.db_path)])
        act_src.reset()

        # Read gen page in RESTORED database, convert it to hex and check whether generator values can be found there or no.
        # Expected result: NOT for all values because DB was encrypted.
        # ~~~~~~~~~~~~~~~~
        check_page_for_readable_values(act_res.db.db_path, gen_page_number, pg_size, check_sequence_values, True, 'FINAL. ')

        act_src.expected_stdout = 'FINAL. ' + SUCCESS_MSG
        act_src.stdout = capsys.readouterr().out
        assert act_src.clean_stdout == act_src.clean_expected_stdout
        act_src.reset()

    #< with act_src.db.connect()

    # Final check: ensure that sequences have proper values:
    ##############
    SQL_SCHEMA_PREFIX = '' if act_res.is_version('<6') else  'PUBLIC.'
    expected_stdout = f"""
        Generator {SQL_SCHEMA_PREFIX}GEN_7FFFFFFF, current value: 2147483646, initial value: 2147483647, increment: 1
        Generator {SQL_SCHEMA_PREFIX}GEN_BA0BAB, current value: 12192682, initial value: 12192683, increment: 1
        Generator {SQL_SCHEMA_PREFIX}GEN_BADF00D, current value: 195948556, initial value: 195948557, increment: 1
        Generator {SQL_SCHEMA_PREFIX}GEN_C0FFEE, current value: 12648429, initial value: 12648430, increment: 1
        Generator {SQL_SCHEMA_PREFIX}GEN_CACA0, current value: 830623, initial value: 830624, increment: 1
        Generator {SQL_SCHEMA_PREFIX}GEN_DEC0DE, current value: 14598365, initial value: 14598366, increment: 1
        Generator {SQL_SCHEMA_PREFIX}GEN_DECADE, current value: 14600925, initial value: 14600926, increment: 1
    """

    act_res.expected_stdout = expected_stdout
    act_res.isql(switches = ['-q'], input = 'show sequ;', combine_output = True, io_enc = locale.getpreferredencoding())
    assert act_res.clean_stdout == act_res.clean_expected_stdout
    act_res.reset()
