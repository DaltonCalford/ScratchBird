#coding:utf-8

"""
ID:          functional.blob.test_access_to_blob_in_multiple_cursors.py
TITLE:       Ability to read same blob_id using multiple cursors
DESCRIPTION: 
    Source ticket:
    https://github.com/FirebirdSQL/firebird/pull/8318

    Test creates small file and writes random data in it.
    Then it opens streamed blob, stores file content in DB and attempts to access this blob using two cursors
    by creating blob_readers, call read() and comparing fetched data.
    Closing of second blob_reader must PASS w/o error (it raises 'invalid BLOB handle' before fix).
    Above mentioned actions are done for several sizes, see DATA_LEN_LIST.

    If all fine then only 'dummy' message is printed about success.
    Otherwise we use capsys.readouterr().out that accumulated print() output and show all detailed info.
NOTES:
    [29.03.2025] pzotov
    Problem with access to same blob using two cursors was fixed in:
    https://github.com/FirebirdSQL/firebird/commit/48ef37d826454eff70057c17b414c19e0f00f8df

    Confirmed bug on 6.0.0.698-f5c7e57: got 'invalid BLOB handle' when trying to close second blob_reader.
    Checked on 6.0.0.702-16ef06e -- all OK.

    [02.04.2025] pzotov
    Checked on 5.0.3.1639-f47fcd9 after commit 54c61058 ("Backported seek for cached blob (missed part of #8318)").
    Reduced min_version to 5.0.3.
"""
import os
from pathlib import Path
import random
import traceback
import string

import pytest
from firebird.qa import *
from firebird.driver import DbInfoCode, DatabaseError

EXPECTED_MSG = 'Passed.'
init_ddl = """
    recreate global temporary table bdata(blob_fld blob) on commit preserve rows;
    commit;
"""

db = db_factory(init = init_ddl)
act = python_act('db')

tmp_blob_file = temp_file('tmp_small_blob.dat')

@pytest.mark.version('>=5.0.3')
def test_1(act: Action, tmp_blob_file: Path, capsys):

    DATA_LEN_LIST = \
        (     0,    1,    2,    3,    4,
          32764,32765,32766,32767,32768,
          65532,65533,65534,65535,65536
        )

    with act.db.connect() as con:
        cur1 = con.cursor()
        cur2 = con.cursor()
        cur1.execute("select rdb$get_context('SYSTEM', 'CLIENT_VERSION') as client_version from rdb$database")
        hdr=cur1.description
        for r in cur1:
            for i in range(0,len(hdr)):
                print( hdr[i][0],':', r[i] )
        print(f'{con.info.version=}, {con.info.get_info(DbInfoCode.PROTOCOL_VERSION)=}')
        failed_cnt = 0
        for b_gen_size in DATA_LEN_LIST:
            print(f'\n\nStart of loop for {b_gen_size=} in DATA_LEN_LIST')
            try:
                b_data_1, b_data_2 = '', ''
                with open(tmp_blob_file, 'wb') as f_binary_data:
                    # f_binary_data.write( os.urandom(b_gen_size) )
                    f_binary_data.write( bytearray( ''.join(random.choices(string.ascii_letters, k = b_gen_size)).encode('ascii') ) )

                with open(tmp_blob_file, 'rb') as f_binary_data:
                    cur1.execute("delete from bdata")
                    cur1.execute("insert into bdata(blob_fld) values (?)", (f_binary_data,))
                    con.commit()
                    
                    cur1.stream_blobs.append('BLOB_FLD')
                    cur1.execute('select blob_fld from bdata')
                    blob_reader_1 = cur1.fetchone()[0]
                    blob_reader_1.seek(0, os.SEEK_SET)
                    b_data_1 = blob_reader_1.read(b_gen_size)

                    cur2.stream_blobs.append('BLOB_FLD')
                    cur2.execute('select blob_fld from bdata')
                    blob_reader_2 = cur2.fetchone()[0]
                    blob_reader_2.seek(-b_gen_size, os.SEEK_END)
                    b_data_2 = blob_reader_2.read(b_gen_size)
                    
                    blob_reader_1.close()

                    # before fix this caused 'invalid BLOB handle' (gdscode = 335544328):
                    blob_reader_2.close()

                if b_gen_size > 0 and (not b_data_1 or not b_data_2) or b_data_1 != b_data_2:
                    print(f'{b_gen_size=}. Data fetched in different cursors are not equal: {b_data_1=}, {b_data_2=}.')
                    failed_cnt += 1
                else:
                    print(f'{b_gen_size=}. Data fetched in different cursors EQUALS: {b_data_1=}, {b_data_2=}.')
            except DatabaseError as e:
                print(e)
                print(e.gds_codes)
                for x in traceback.format_exc().split('\n'):
                    print('  ',x)


    if failed_cnt == 0:
        act.stdout = EXPECTED_MSG
    else:
        act.stdout = 'CHECK NOT PASSED:\n\n' + capsys.readouterr().out

    assert act.clean_stdout == EXPECTED_MSG
