#coding:utf-8

"""
ID:          issue-6412
ISSUE:       6412
TITLE:       Generator pages are not encrypted
DESCRIPTION:
   Several sequences are created in this test.
   Then we obtain generator page ID and page size by querying RDB$PAGES and MON$DATABASE tables.
   After this, we check that values of sequences *PRESENT* in NON-encrypted database by opening DB file in 'rb' mode
   and reading content of its generator page.
   Further, we run 'ALTER DATABASE ENCRYPT ...' and wait for until engine complete encryption.
   Finally, we read generator page again. NO any value of secuences must be found at this point.

   Confirmed non-encrypted content of generators page on: 4.0.0.1627; 3.0.5.33178.
   Checked on: 4.0.0.1633: OK, 2.260s; 3.0.5.33180: OK, 1.718s.

JIRA:        CORE-6163
FBTEST:      bugs.core_6163
NOTES:
     [18.06.2022] pzotov
     ::: NB :::
     Creating sequence with some starting value (say, 1000) reflects DIFFERENTLY in low-level storage in FB 4.x+ vs prev. versions.
     In FB 4.x generators page will contain value decremented by 1 (999). Rather, in FB 3.x this value will be stored 'as is' (1000).
     Because of this, we must *not* put into expected_stdout any concrete values of sequences. All we need is only to verify that
     some (known) generator value contains in the page binary content before encryption, and can not be found there when DB encrypted
     (see func 'show_gen_page_readable_outcome').

     Above mentioned change in FB 4.x became act since build 4.0.0.2131 (06-aug-2020).
     statement 'alter sequence <seq_name> restart with 0' changes rdb$generators.rdb$initial_value to -1 thus next call
     gen_id(<seq_name>,1) will return 0 (ZERO!) rather than 1.
     See also CORE-6084 and its fix: https://github.com/FirebirdSQL/firebird/commit/23dc0c6297825b2e9006f4d5a2c488702091033d

     Checked on 4.0.1.2692, 3.0.8.33535.
"""
import os
import binascii
import datetime as py_dt
from datetime import timedelta
import time
import configparser
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

init_ddl = """
    create sequence gen_ba0bab start with 12192683;
    create sequence gen_badf00d start with 195948557;
    create sequence gen_caca0  start with 830624;
    create sequence gen_c0ffee start with 12648430;
    create sequence gen_dec0de start with 14598366;
    create sequence gen_decade start with 14600926;
    create sequence gen_7FFFFFFF start with 2147483647;
"""
db = db_factory(init = init_ddl)
act = python_act('db', substitutions=[('[ \t]+', ' ')])

#-----------------------------------------------------------------------------------------------------
def show_gen_page_readable_outcome(dbname, gen_page_number, pg_size, current_seq_name_val_dict):

    db_handle = open( dbname, "rb")
    db_handle.seek( gen_page_number * pg_size )
    page_content = db_handle.read( pg_size )
    # read_binary_content( db_handle, gen_page_number * pg_size, pg_size )
    db_handle.close()
    page_as_hex=binascii.hexlify( page_content )

    # Iterate for each sequence value:
    for k,v in sorted(current_seq_name_val_dict.items()):

        # Get HEX representation of digital value.
        # NOTE: format( 830624, 'x') is 'caca0' contains five (odd number!) characters.
        hex_string = format(abs(v),'x')

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

        print(k, 'FOUND.' if n_as_reversed_hex in page_as_hex else 'NOT FOUND.' )

#-----------------------------------------------------------------------------------------------------

expected_stdout = """
    GEN_7FFFFFFF FOUND.
    GEN_BA0BAB   FOUND.
    GEN_BADF00D  FOUND.
    GEN_C0FFEE   FOUND.
    GEN_CACA0    FOUND.
    GEN_DEC0DE   FOUND.
    GEN_DECADE   FOUND.

    GEN_7FFFFFFF NOT FOUND.
    GEN_BA0BAB   NOT FOUND.
    GEN_BADF00D  NOT FOUND.
    GEN_C0FFEE   NOT FOUND.
    GEN_CACA0    NOT FOUND.
    GEN_DEC0DE   NOT FOUND.
    GEN_DECADE   NOT FOUND.
"""

@pytest.mark.encryption
@pytest.mark.version('>=3.0.5')
def test_1(act: Action, capsys):

    encryption_started = False
    encryption_finished = False

    with act.db.connect() as con:
        cur=con.cursor()
        get_current_seq_values='''
            execute block returns( gen_name rdb$generator_name, gen_curr bigint) as
                declare g_incr smallint;
            begin
                for
                    select trim(rdb$generator_name)
                    from rdb$generators
                    where rdb$system_flag is distinct from 1
                    order by rdb$generator_id
                    into gen_name
                do begin
                    execute statement 'select gen_id(' || gen_name || ', 0) from rdb$database'  into gen_curr;
                    suspend;
                end
            end
        '''

        # Obtain current values of user generators:
        cur.execute(get_current_seq_values)
        current_seq_name_val_dict={}
        for r in cur:
            current_seq_name_val_dict[ r[0] ] = r[1]


        # Obtain page size and number of generators page:
        cur.execute('select m.mon$page_size,min(rdb$page_number) from mon$database m cross join rdb$pages p where p.rdb$page_type = 9 group by 1')
        pg_size, gen_page_number = -1,-1
        for r in cur:
            pg_size=r[0]
            gen_page_number=r[1]


        # Read gen page, convert it to hex and check whether generator values can be found there or no:
        # Expected result: YES for all values because DB not encrypted now.
        # ~~~~~~~~~~~~~~~
        show_gen_page_readable_outcome(act.db.db_path, gen_page_number, pg_size, current_seq_name_val_dict)
        
        #---------------------------------------
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
    

    if encryption_finished:
        # Read again gen page, convert it to hex and check whether generator values can be found there or no.
        # Expected result: NOT for all values because DB was encrypted.
        # ~~~~~~~~~~~~~~~~
        show_gen_page_readable_outcome(act.db.db_path, gen_page_number, pg_size, current_seq_name_val_dict)
    else:
        print('Nothing to be tested: DB not encrypted.')

    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
