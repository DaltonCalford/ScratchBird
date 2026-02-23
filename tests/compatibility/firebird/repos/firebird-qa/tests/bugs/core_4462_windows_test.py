#coding:utf-8

"""
ID:          issue-4782
ISSUE:       4782
TITLE:       Make it possible to restore compressed .nbk files without explicitly decompressing them
DESCRIPTION:
    Test uses two preliminarily created .zip files:
        * compressor-7zip-standalone-exe.zip -- contains standalone 7zip.exe
        * compressor-zstd-standalone-exe.zip -- contains standalone zstd.exe

    We extract files from these .zip archives and for EACH of them do following:
        * define max level of nbackup files which will be created, see 'MAX_NBK_LEVEL';
          if MAX_NBK_LEVEL == 2 then THREE nbk files will be created: .nbk0, nbk1, nbk2; etc
        * make LOOP for <MAX_NBK_LEVEL+1> iterations, with adding some data on each iteration into a test table
          (we insert int, bigint, double precision, varchar and blob data; blobs have length more than 1 Mb);
        * invoke "nbackup -b <level_i>" with redirection data to STDOUT stream, so that it can serve as source
          for compressor (7zip or std). We use Python subprocess.communicate() instead of PIPE in order to make
          data from nbackup being passed to compressor STDIN. No .nbk file will be created here, data will be
          stored immediately to compressed file.
        * after finishing loop for <MAX_NBK_LEVEL+1> iterations, we will have appropriate number of compressed
          files with nbackup data. Further, we have to DECOMPRESS data from them using 7zip or zstd ability
          to send data to STDOUT. Firebird NBACKUP utility must be invoked with '-decompress' switch now (making it
          accept data from STDIN).
        * Finally, we run validation of just restored database and compare data with those which source DB.

    This is *initial* implementation! We use trivial database with ascii-only metadata and data.
    Later this test may be expanded for check non-ascii metadata and/or data.
JIRA:        CORE-4462
FBTEST:      bugs.core_4462
NOTES:
    [29.08.2022] pzotov
    1. Implementation of this test for LINUX will differ. It was decided to put it in separate file.
    2. To make test more complex, database is encrypted before actions; before run this test,
       make sure that firebird.conf contains parameter:
           KeyHolderPlugin = fbSampleKeyHolder
       (NB: restored database must also be encrypted, but currently this is not checked by test)

    Checked on 5.0.0.691, 4.0.1.2692 (SS/CS), 3.0.8.33535 (SS/CS).
"""
import os
import locale
import zipfile
from typing import List
import subprocess
from subprocess import PIPE
import re
from pathlib import Path
from difflib import unified_diff
import time

import pytest
from firebird.qa import *
from firebird.driver import SrvRepairFlag, DatabaseError

tmp_blob_txt = temp_file('core_4462_txt.dat')
tmp_blob_bin = temp_file('core_4462_bin.dat')
tmp_rest_log = temp_file('core_4462_res.log')

# Length of generated blob files to be loaded into the table using stream API:
DATA_LEN = 1024*1024

# Last level of nbackups, starting from backup-0:
MAX_NBK_LEVEL = 3
tmp_zipped_nbk_list = temp_files( [ f'tmp_core_4462.nbk{i}.compressed' for i in range(MAX_NBK_LEVEL+1) ] )

tmp_compressors_list = temp_files(['7zip.exe', 'zstd.exe'])

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
enc_settings = QA_GLOBALS['encryption']

ENCRYPTION_PLUGIN = enc_settings['encryption_plugin'] # fbSampleDbCrypt
ENCRYPTION_KEY = enc_settings['encryption_key'] # Red

init_ddl = """
    recreate table test(
        id int primary key
        ,n int
        ,m bigint
        ,x double precision
        ,s varchar(36)
        ,d date
        ,t timestamp
        ,b1 blob
        ,b2 blob
    );
"""
db = db_factory(init = init_ddl, charset='UTF8')
act = python_act('db')

tmp_rest_fdb = db_factory(filename = 'core_4462_res.fdb', do_not_create = True)
act_rest_fdb = python_act('tmp_rest_fdb')

#------------------------------------------------------------------
# Callback function: capture output of full validation (must be empty):
def print_validation(line: str) -> None:
    if line.strip():
        print(f'VALIDATION LOG: {line.upper()}')
#------------------------------------------------------------------

@pytest.mark.encryption
@pytest.mark.version('>=3.0.5')
@pytest.mark.platform('Windows')
def test_1(act: Action, act_rest_fdb: Action, tmp_zipped_nbk_list: List[Path], tmp_compressors_list: List[Path], tmp_blob_txt: Path, tmp_blob_bin: Path, tmp_rest_fdb: Path, tmp_rest_log: Path, capsys):

    with act.db.connect() as con:
        sttm = f'alter database encrypt with "{ENCRYPTION_PLUGIN}" key "{ENCRYPTION_KEY}"'
        try:
            con.execute_immediate(sttm)
            con.commit()
            time.sleep(2)
        except DatabaseError as e:
            print( e.__str__() )

    assert '' == capsys.readouterr().out

    for compressor_binary in tmp_compressors_list:
       zname = compressor_binary.stem # '7zip' ; 'zstd'
       zipped_file = zipfile.Path(act.files_dir / f'compressor-{zname}-standalone-exe.zip', at = f'{compressor_binary.name}')
       compressor_binary.write_bytes(zipped_file.read_bytes())
       # Result: '7zip.exe', 'zstd.exe' - extracted to temp dir.
    
       for iter,tmp_zipped_nbk_i in enumerate(tmp_zipped_nbk_list):
           tmp_blob_txt.write_bytes( ( f'{iter}' * DATA_LEN).encode() )
           tmp_blob_bin.write_bytes( bytearray(os.urandom(DATA_LEN)) )

           insert_sttm = f"""
               insert into test(id, n, m, x, s, d, t, b1, b2)
               values( {iter}
                       ,rand()*2147483647
                       ,rand()*9223372036854775807
                       ,rand() * exp(700)
                       ,uuid_to_char(gen_uuid())
                       ,current_date
                       ,current_timestamp
                       ,?
                       ,?
                     )
           """
           with open(tmp_blob_txt, 'rb') as blob_file_txt, open(tmp_blob_bin, 'rb') as blob_file_bin:
               with act.db.connect() as con:
                   with con.cursor() as cur:
                       if iter == 0:
                           cur.execute('delete from test')

                       # Load stream blob into table: open file with data and pass file object into INSERT command.
                       # https://firebird-driver.readthedocs.io/en/latest/usage-guide.html?highlight=blob#working-with-blobs
                       #cur.execute(f"insert into test(id, n, m, x, s, b1, b2) values({iter}, rand()*2147483647, rand()*9223372036854775807, rand * pi(), uuid_to_char(gen_uuid()), ?, ?)", (blob_file_txt, blob_file_bin,) )
                       cur.execute( insert_sttm, (blob_file_txt, blob_file_bin,) )
                   con.commit()

           # Name of .fbk how it will be seen in the compressed file.
           # NOTE: this file will *not* be created on disk during 'nbackup -b ...' execution:
           nbk_iter = Path(os.path.splitext(act.db.db_path)[0] + f".nbk{iter}")
           nbk_iter.unlink(missing_ok = True)
           tmp_zipped_nbk_i.unlink(missing_ok = True) # "/path/to/tmpdir/tmp_core_4462.nbk<i>.compressed"

           # NBACKUP -> COMPRESSOR, without creating .nbk files and using subprocess.communicate() instead of PIPE:
           # nbackup -b <level> nbk_full_path_and_name stdout | 7zip u -si<nbk_fileonly_name> <compressed_name.7z>
           # nbackup -b <level> nbk_full_path_and_name stdout | zstd -q -f -o <compressed_name.zst>
           p_sender = subprocess.Popen([act.vars['nbackup'], '-b', f'{iter}', act.db.db_path, 'stdout', '-user', act.db.user], stdout=PIPE)
           if compressor_binary.stem == 'zstd':
               p_getter = subprocess.Popen( [ compressor_binary, '--quiet', '--force', '-o', tmp_zipped_nbk_i ], stdin = p_sender.stdout, stdout = PIPE )
           else:
               # -bb[0-3] : set output log level
               # -bt : show execution time statistics
               # -bs{o|e|p}{0|1|2} : set output stream for output/error/progress line
               #p_getter = subprocess.Popen( [ compressor_binary, 'u', '-si' + nbk_iter.name, tmp_zipped_nbk_i ], stdin = p_sender.stdout, stdout = PIPE )
               p_getter = subprocess.Popen( [ compressor_binary, 'u', '-si', tmp_zipped_nbk_i ], stdin = p_sender.stdout, stdout = PIPE )

           p_sender.stdout.close()
           p_getter_stdout, p_getter_stderr = p_getter.communicate()

       #<for iter,tmp_zipped_nbk_i in enumerate(tmp_zipped_nbk_list)

       # Store current data of TEST table in order to compare it later (after restoring from compressed .nbk files):
       chk_query = 'select id, n, m, x, s, d, t, hash(b1), hash(b2) from test order by id'
       expected_data = []
       with act.db.connect() as con:
           with con.cursor() as cur:
               cur.execute(chk_query)
               expected_data = cur.fetchall()

       with open(tmp_rest_log, 'w') as f:
           act_rest_fdb.db.db_path.unlink(missing_ok = True)
           # nbackup -decompress "7zip x -y -so" -restore ...
           # nbackup -decompress "zstd -dc --no-progress" -restore ...
           if compressor_binary.stem == 'zstd':
               compressor_extract_command = ' '.join( (str(compressor_binary), '--decompress', '--quiet', '--force', '--stdout') )
           else:
               compressor_extract_command = ' '.join( (str(compressor_binary),'x', '-y', '-so') )

           subprocess.run( [ act.vars['nbackup']
                             ,'-decompress'
                             ,compressor_extract_command
                             ,'-restore', act_rest_fdb.db.db_path
                           ] + tmp_zipped_nbk_list
                           , stdout = f, stderr = subprocess.STDOUT)

       if tmp_rest_log.stat().st_size > 0:
           # Example of log when some compressed .nbk is broken:
           # Invalid incremental backup file: ...
           # DE> zstd: error 70 : Write error : cannot write decoded block : Broken pipe 
           print('Restore failed, check log:')
           with open(tmp_rest_log, 'r') as f:
               for line in f:
                   if line.strip():
                       print(line)

       assert '' == capsys.readouterr().out

       #----------------------------------------------------------------------------

       # Get FB log before validation, run validation and get FB log after it.
       # Difference must be have only text about zero errors/warnings/fixed:
       with act_rest_fdb.connect_server() as srv:

           fblog_1 = act_rest_fdb.get_firebird_log()
           srv.database.repair(database = act_rest_fdb.db.db_path, flags=SrvRepairFlag.CORRUPTION_CHECK)
           fblog_2 = act_rest_fdb.get_firebird_log()

           p_diff = re.compile('Validation finished: \\d+ errors, \\d+ warnings, \\d+ fixed')
           validation_result = ''
           for line in unified_diff(fblog_1, fblog_2):
               if line.startswith('+') and p_diff.search(line):
                   validation_result =line.strip().replace('\t', ' ')
                   break

           assert validation_result == '+ Validation finished: 0 errors, 0 warnings, 0 fixed'
           act_rest_fdb.reset()

       #----------------------------------------------------------------------------

       # check that data in the restored DB exactly the same as in the source DB:
       restored_data = []
       with act_rest_fdb.db.connect() as con:
           with con.cursor() as cur:
               cur.execute(chk_query)
               restored_data = cur.fetchall()

       if not (expected_data and restored_data and len( set(expected_data + restored_data) ) == MAX_NBK_LEVEL+1):
           print('Either some data were lost or differ:')
           print('expected_data:')
           for x in expected_data:
               print(x)
           print('restored_data:')
           for x in restored_data:
               print(x)
      
       assert '' == capsys.readouterr().out

