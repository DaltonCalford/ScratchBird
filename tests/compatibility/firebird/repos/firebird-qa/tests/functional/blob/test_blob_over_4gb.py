#coding:utf-8

"""
ID:          functional.blob.test_blob_over_4gb.py
TITLE:       Check ability to operate with blob with size greater than 4Gb
DESCRIPTION: 
    Test generated binary file with random data and size = 4Gb + 1 bytes (see 'BLOB_DATA_LEN').
    In order to reduce memory consumption, generated data is splitted onto CHUNKS
    with size = 4Gb / 1000 (see 'DATA_CHUNK_LEN'), and writing to the file is done by such small portions.
    During generation, hashes are evaluated for each chunk; they are added to the array (see 'chunk_hashes_lst').

    Then we do several operations with DB in order to check their results:
        * run 'nbackup -L' (return_code must be 0);
        * open generated file as blob_stream and insert its content to the view v_data;
        * read just inserted blob (also as blob stream) and compare its hash with hash of original data.
          We read this blob by small portions (chunks) and evaluate hash for each gathered part.
          All such hashes are accumulated in array (see 'check_hashes_lst') and then lists are compared.
          Comparison of two lists must have result = True, size of obtained data must equal to BLOB_DATA_LEN
        * run 'nbackup -N' (return_code must be 0);
        * run 'gstat -r' and parse some lines of its output (rows with blob info and table size):
          gstat output must have:
          **  two lines with info about blob, like these:
             Blobs: 1, total length: 4294967297, blob pages: 526345
                 Level 2: 1, total length: 4294967297, blob pages: 526345
             (and 'total length' must be equal BLOB_DATA_LEN);
          ** one line with info about table size:
             Table size: 4311842816 bytes
             (and size must be greater than BLOB_DATA_LEN).
NOTES:
    [18.03.2025] pzotov
    1. ### ACHTUNG ### At least 8.1 Gb of free storage required for generated data and test DB used by this test.
       CHECK folder that is specified in '--basetemp' switch of pytest command!
    2. Usual backup and restore *was* checked but duration of test lasts too long in this case (about 10 minutes).
       Because of this, b/r currently is DISABLED.
    3. Also, it was checked wire statistics when blob data was obtained in ISQL:
        Wire logical statistics:
          send packets =   262186
          recv packets =   262184
          send bytes   =  4195080
          recv bytes   = 4303881656
        Wire physical statistics:
          send packets =   262180
          recv packets =   786533
          send bytes   =  4195080
          recv bytes   = 4303881656
          roundtrips   =   262179
       But this call also was DISABLED because it takes too much time.

    Discussed with dimitr, letters since 15-mar-2025 02:16.
    Test execution time: about 5 minutes.
    Checked on 6.0.0.677 SS/CS.
"""

import os
import hashlib
import re
import locale
from pathlib import Path
import time

import pytest
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

init_ddl = """
   set term ^;
   recreate view v_data as select 1 x from rdb$database
   ^ 
   recreate view v_vchr as select 1 x from rdb$database
   ^ 
   recreate view v_blob as select 1 x from rdb$database
   ^ 
   recreate table test_vchr(id int primary key using index test_vchr_pk, text_fld varchar(1))
   ^
   recreate table test_blob(id int primary key using index test_blob_pk, blob_fld blob)
   ^
   recreate sequence gen_test
   ^
   recreate view v_vchr as select id, text_fld from test_vchr
   ^
   recreate view v_blob as select id, blob_fld from test_blob
   ^
   recreate view v_data as select a.id, a.text_fld, b.blob_fld from test_vchr a left join test_blob b on a.id = b.id
   ^
   create or alter trigger v_data_biud for v_data active before insert or update or delete position 0
   as
       declare v_id int;
   begin
       if (inserting) then
           begin
               insert into v_vchr(id, text_fld) values( coalesce( new.id, gen_id(gen_test,1) ), new.text_fld )
               returning id into v_id;
               insert into v_blob(id, blob_fld) values( :v_id, new.blob_fld );
           end
       else if (updating) then
           begin
               update v_vchr set id = coalesce(new.id, old.id), text_fld = :new.text_fld where id = old.id;
               update v_blob set id = coalesce(new.id, old.id), blob_fld = :new.blob_fld where id = old.id;
           end
       else
          begin
              delete from v_vchr where id = old.id;
              delete from v_blob where id = old.id;
     	  end
   end
   ^
   commit
   ^
"""

db = db_factory(init = init_ddl)
act = python_act('db', substitutions = [(r'blob pages: \d+', 'blob pages'), (r'Level(:)?\s+\d+:\s+\d+, ', 'Level, ')])

BLOB_DATA_LEN = 2**32+1
DATA_CHUNK_LEN = BLOB_DATA_LEN // 1000

tmp_dat = temp_file('tmp_blob_4gb.dat')
tmp_fbk = temp_file('tmp_blob_4gb.fbk')

@pytest.mark.version('>=6.0')
def test_1(act: Action, tmp_dat: Path, tmp_fbk: Path, capsys):

    chunk_hashes_lst = []
    with open(tmp_dat, 'wb+') as f_blob_data:
        n = 0
        while n < BLOB_DATA_LEN:
            rnd_chunk = os.urandom(min(DATA_CHUNK_LEN, BLOB_DATA_LEN-n))
            chunk_hashes_lst.append( hashlib.sha1(rnd_chunk).hexdigest() )
            f_blob_data.write( rnd_chunk )
            n += DATA_CHUNK_LEN

    fdb_delta = act.db.db_path.with_suffix('.delta')
    fdb_delta.unlink(missing_ok = True)

    #-----------------------------------------
    act.nbackup(switches=['-L', act.db.dsn], combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.return_code == 0, f'nbackup -L failed: {act.clean_stdout}'
    act.reset()
    #-----------------------------------------

    with act.db.connect() as con:
        cur = con.cursor()
        with open(tmp_dat, 'rb') as f_blob_data:
            cur.execute("insert into v_data(text_fld, blob_fld) values (?, ?)", ('a', f_blob_data))
            con.commit()
        tmp_dat.unlink()
        #-------------------------------------
        cur.stream_blobs.append('BLOB_FLD')
        cur.execute('select blob_fld, octet_length(blob_fld) from v_data order by id rows 1')
        b_reader, b_length = cur.fetchone()
        n = 0
        check_hashes_lst = []
        while n < BLOB_DATA_LEN:
            b_data_chunk = b_reader.read( min(DATA_CHUNK_LEN, BLOB_DATA_LEN-n) )
            check_hashes_lst.append( hashlib.sha1(b_data_chunk).hexdigest() )
            n += DATA_CHUNK_LEN
        print(f'Fetch finished, {b_length=}. Equality check:', chunk_hashes_lst == check_hashes_lst)

    act.expected_stdout = f"""
        Fetch finished, {b_length=}. Equality check: True
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    #---------------------------------------------------
    act.nbackup(switches=['-N', act.db.dsn], combine_output = True, io_enc = locale.getpreferredencoding())
    assert act.return_code == 0, f'nbackup -N failed: {act.clean_stdout}'
    act.reset()
    #---------------------------------------------------

    act.gstat(switches=['-r', '-t', 'TEST_BLOB'])
    
    blob_page_ptn = re.compile(r'total length(:)?\s+\d+, blob pages(:)?\s+\d+')
    table_size_ptn = re.compile(r'Table size(:)?\s+\d+\s+bytes')
    table_size_parsing_result = ''
    expected_table_size = f'Table size: NOT LESS than {BLOB_DATA_LEN}'
    for line in act.stdout.splitlines():
        if blob_page_ptn.search(line):
            print(line)
        if table_size_ptn.search(line):
            table_size_value = int(line.split()[2])
            if table_size_value > BLOB_DATA_LEN:
                table_size_parsing_result = expected_table_size
            else:
                table_size_parsing_result = f'Table size: INCORRECT, {table_size_value} - less than {BLOB_DATA_LEN}'
            print(table_size_parsing_result)

    act.stdout = capsys.readouterr().out
    act.expected_stdout = f"""
        Blobs: 1, total length: {BLOB_DATA_LEN}, blob pages
        Level, total length: {BLOB_DATA_LEN}, blob pages
        {expected_table_size}
    """
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    '''
        D I S A B L E D:   T O O    L O N G    T I M E
        #----------------------------------------------------
        expected_backup_out = 'Backup completed OK.'
        expected_restore_out = 'Restore completed OK.'
        successful_backup_pattern = re.compile(r'gbak:closing file, committing, and finishing. \\d+ bytes written', re.IGNORECASE)
        successful_restore_pattern = re.compile( r'gbak:finishing, closing, and going home', re.IGNORECASE )

        act.gbak(switches = ['-b', '-verbint', str(2**31-1), act.db.dsn, tmp_fbk])
        for line in act.stdout.splitlines():
            if successful_backup_pattern.search(line):
                print(expected_backup_out)
                break

        act.expected_stdout = f"""
            {expected_backup_out}
        """
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
        #----------------------------------------------------
        act.gbak(switches = ['-rep', '-verbint', str(2**31-1), tmp_fbk, act.db.dsn])
        for line in act.stdout.splitlines():
            if successful_restore_pattern.search(line):
                print(expected_restore_out)
                break

        act.expected_stdout = f"""
            {expected_restore_out}
        """
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
    '''
