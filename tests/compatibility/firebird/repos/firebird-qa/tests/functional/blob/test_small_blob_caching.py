#coding:utf-8

"""
ID:          functional.blob.test_small_blob_caching.py
TITLE:       Operations with inline streamed blobs when boundary values are used for seek() and read() methods.
DESCRIPTION: 
    Source ticket:
    https://github.com/FirebirdSQL/firebird/pull/8318

    Test verifies that handling of data stored in file and in database blob field leads to same result.
    This is done for several scopes of size which are close to 0, 1, 8, 16, 32 and 64K (see 'CHECKED_LEN_RANGES').
    For each scope of sizes following actions are performed:
        * start outer loop for size, from low to high bound (see: 'for b_gen_size in range(..., ...)');
        * create random string of size <b_gen_size> and store it in the file and blob field (using stream API);
        * let scope boundary values be: 'pos_beg', 'pos_end' (they will be used in evaluation of seek() arg.);
        * start inner loop: 'for i_pos in range(pos_beg, pos_end+1)' and use counter (i_pos) as argument for seek();
        * adjust position of file pointer using seek();
        * calculate number of bytes to be gathered after seek(): b_read_cnt = pos_end+1 - i_pos;
        * read <b_read_cnt> bytes from file with data;
        * do the same for blob that is stored in DB;
        * compare content of bytearrays that was gathered from FILE vs BLOB. They must be equal.
    These actions are done for all values of 'whence' argument for seek() call: os.SEEK_SET, os.SEEK_CUR, os.SEEK_END
    If checks passed for all iterations then only 'dummy' message is printed about success.
    Otherwise we use capsys.readouterr().out that accumulated print() output and show all detailed info.
NOTES:
    [27.03.2025] pzotov
    0. ### ACHTUNG ###
       Source code of Python firebird-driver has to be checked/adjusted in order this test runs correctly!
       In class Cursor we have to check that its method '_unpack_output()' contains in if-else branch which checks
       'datatype == SQLDataType.BLOB' following line:
           blob = self._connection._att.open_blob(self._transaction._tra, blobid) // note: last argument is 'blobid'
                                                                          ~~~~~~
       Originally this line looked like this:
           blob = self._connection._att.open_blob(self._transaction._tra, blobid, _bpb_stream)
                                                                                  ~~~~~~~~~~~
       This must be considered as BUG because kind of blob (stream vs materialized) can be set only when blob is CREATED
       but NOT when it is to be fetched.
       Opening blob with BPB (i.e. with argument '_bpb_stream') forces client to remove previously cached inline blob
       (because it was opened without BPB). This, in turn, causes seek() to work with NON-cached blob and test in such case
       will not check what we need.

       This issue was found by Vlad, letter 25.03.2025 00:13.

    1. Test tries to read content both within data and beyond its end!
       For example, following occurs when use b_gen_size = 4 and call seek() with requirement to set file position
       before first byte of file/blob:
           data:      |qwer|
           i_pos = 0: *12345       // seek(0, whence = os.SEEK_BEG); read(5) --> result must be bytearray with len = 4
           i_pos = 1: |*12345      // seek(1, whence = os.SEEK_BEG); read(5) --> result must be bytearray with len = 3
           i_pos = 2: | *12345     // seek(2, whence = os.SEEK_BEG); read(5) --> result must be bytearray with len = 2
           i_pos = 3: |  *12345    // seek(3, whence = os.SEEK_BEG); read(5) --> result must be bytearray with len = 1
           i_pos = 4: |   *12345   // seek(4, whence = os.SEEK_BEG); read(5) --> result must be bytearray with len = 0
       Legend:
           | = boundaries (before start and after end of data)
           * = position for seek
           12345 = subsequent count of bytes that we try to gather into bytearray
       After all values of i_pos are checked, we switch whence from os.SEEK_BEG to os.SEEK_CUR, i.e. use relative offsets.
       In this case we start from moving pointer back from current position to the beginning of file/blob, and repeat the same.
       After all values of i_pos for os.SEEK_CUR, we check the same for os.SEEK_END, but one need to take in account that
       we can not use negative values of b_read_pos with ABS() greater than b_gen_size (e.g. b_read_pos=-5 when b_gen_size=4),
       because it causes Python exception.
    2. During test implementation, it was discovered that comparison of FILE content vs DB blob field can fail if size of
       generated blobs are about 32K and blob is stored in GLOBAL TEMPORARY TABLE. It was fixed by commit:
       https://github.com/FirebirdSQL/firebird/commit/51b72178c29331ccb33e38f0c36f4d0cee902a7c
       ("Don't rely on wrong assumption that stream blob always have maximum possible segment size").
       Because of that, test intentionally uses GTT instead of permanent table to store blobs.
    
    Checked on intermediate snapshot 6.0.0.698-f5c7e57.

    [02.04.2025] pzotov
    Checked on 5.0.3.1639-f47fcd9 after commit 54c61058 ("Backported seek for cached blob (missed part of #8318)").
    Reduced min_version to 5.0.3.
"""
import os
from pathlib import Path
import random
import string

import pytest
from firebird.qa import *
from firebird.driver import DbInfoCode

CHECKED_LEN_RANGES = {
    'about_00k' : (0, 5)
   ,'about_01k' : (   1024-4,    1024+1)
   ,'about_08k' : ( 8*1024-4,  8*1024+1)
   ,'about_16k' : (16*1024-4, 16*1024+1)
   ,'about_32k' : (32*1024-4, 32*1024+1)
   ,'about_64k' : (64*1024-4, 64*1024+1)
}

EXPECTED_MSG = 'All checks passed.'
init_ddl = """
    recreate global temporary table bdata(blob_fld blob) on commit preserve rows;
    -- recreate table bdata(blob_fld blob);
    recreate view v_bdata as select blob_fld as v_blob_fld from bdata;
    commit;
"""

db = db_factory(init = init_ddl)
act = python_act('db')

tmp_blob_file = temp_file('tmp_small_blob.dat')

@pytest.mark.version('>=5.0.3')
def test_1(act: Action, tmp_blob_file: Path, capsys):
    with act.db.connect() as con:
        cur1 = con.cursor()
        cur1.execute("select rdb$get_context('SYSTEM', 'CLIENT_VERSION') as client_version from rdb$database")
        hdr=cur1.description
        for r in cur1:
            for i in range(0,len(hdr)):
                print( hdr[i][0],':', r[i] )
        print(f'{con.info.version=}, {con.info.get_info(DbInfoCode.PROTOCOL_VERSION)=}')

        failed_count = 0
        for b_range_mnemona, b_len_checked_range in CHECKED_LEN_RANGES.items():
            print(f'Preparing data, {b_range_mnemona=}, {b_len_checked_range=}')
            for b_gen_size in range(b_len_checked_range[0], b_len_checked_range[1]+1):
                print(f'\nGenerate file with data to be stored further in blob field, {b_len_checked_range=}, {b_gen_size=}')
                with open(tmp_blob_file, 'wb') as f_binary_data:
                    f_binary_data.write( bytearray( ''.join(random.choices(string.ascii_letters + string.digits, k = b_gen_size)).encode('ascii') ) )

                with open(tmp_blob_file, 'rb') as f_binary_data:
                    cur1.execute("delete from bdata")
                    cur1.execute("insert into bdata(blob_fld) values (?)", (f_binary_data,))
                    con.commit()

                    cur1.stream_blobs.append('BLOB_FLD')
                    cur1.execute('select blob_fld from bdata')
                    blob_reader_1 = cur1.fetchone()[0]

                    pos_beg, pos_end = b_len_checked_range[:2]
                    b_read_cnt = 0
                    for i_pos in range(pos_beg, pos_end+1):
                        print(f'\n{i_pos=}', ' - *NOTE* WORK BEYOND EOF:' if i_pos > b_gen_size else '')
                        for whence in (os.SEEK_SET, os.SEEK_CUR, os.SEEK_END):
                            print(f'\n  Start loop for whence in (os.SEEK_SET, os.SEEK_CUR, os.SEEK_END): {f_binary_data.tell()=}, {blob_reader_1.tell()=}')
                            if whence == os.SEEK_CUR:
                                f_binary_data.seek( -min(b_gen_size, (i_pos + b_read_cnt)), os.SEEK_CUR)
                                blob_reader_1.seek( -min(b_gen_size, (i_pos + b_read_cnt)), os.SEEK_CUR)
                                print(f'    whence == os.SEEK_CUR --> move back for {-min(b_gen_size, (i_pos + b_read_cnt))=}. Result: {f_binary_data.tell()=}, {blob_reader_1.tell()=}')
                            elif whence == os.SEEK_END:
                                b_read_pos = -(pos_end+1 - i_pos)
                                print(f'    whence == os.SEEK_END: {b_read_pos=}')
                                if -b_read_pos > b_gen_size:
                                    print(f'    Can not use negative {b_read_pos=} for {whence=} and {b_gen_size=}')
                                    continue
                            else:
                                b_read_pos = i_pos
                                print(f'    whence == os.SEEK_SET: {b_read_pos=}')

                            print(f'    FILE: f_binary_data.seek({b_read_pos}, {whence=})')
                            f_binary_data.seek(b_read_pos, whence)
                            b_read_cnt = pos_end+1 - i_pos
                            print(f'    FILE: {f_binary_data.tell()=}, attempt to read {b_read_cnt=} bytes starting from {i_pos=} using {whence=}')
                            b_data_in_file = f_binary_data.read(b_read_cnt)
                            print(f'    FILE: completed read {b_read_cnt=} bytes starting from {i_pos=} using {whence=}. Result: {f_binary_data.tell()=}, {len(b_data_in_file)=}')

                            # Now do the same against DB (stream blob):
                            print(f'    BLOB: blob_reader_1.seek({b_read_pos}, {whence=})')
                            blob_reader_1.seek(b_read_pos, whence)
                            print(f'    BLOB: {blob_reader_1.tell()=}, attempt to read {b_read_cnt=} bytes starting from {i_pos=} using {whence=}')
                            b_data_from_db = blob_reader_1.read(b_read_cnt)
                            print(f'    BLOB: completed read {b_read_cnt=} bytes starting from {i_pos=} using {whence=}. Result: {blob_reader_1.tell()=}, {len(b_data_from_db)=}')

                            if not b_data_in_file == b_data_from_db:
                                print(f'### ERROR ### Data gathered from FILE differs from BLOB: b_data_in_file: >{b_data_in_file}<, b_data_from_db: >{b_data_from_db}<')
                                failed_count += 1
                            else:
                                print(f'+++ PASSED +++ Data gathered from FILE and BLOB equals: b_data_in_file: >{b_data_in_file}<, b_data_from_db: >{b_data_from_db}<')

                    blob_reader_1.close()

            #< for b_gen_size in range(b_len_checked_range[0], b_len_checked_range[1]+1)
        #< for b_range_mnemona, b_len_checked_range in CHECKED_LEN_RANGES.items()

    if failed_count == 0:
        act.stdout = EXPECTED_MSG
    else:
        act.stdout = 'CHECK NOT PASSED:\n\n' + capsys.readouterr().out

    assert act.clean_stdout == EXPECTED_MSG
