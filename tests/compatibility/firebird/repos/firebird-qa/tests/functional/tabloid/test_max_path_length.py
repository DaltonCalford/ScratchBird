#coding:utf-8

"""
ID:          tabloid.max-path-length
TITLE:       Check ability to create database with total length of path and file name about maximal allowed limit in Windows
DESCRIPTION:
  Maximal TOTAL length of <drive>:/<path>/<DB_filename> on Windows is about 260 characters.
      Firebird can *not* create DB file with such length, attempt to do this will failed with message:
      SQLSTATE = 08001 / I/O error during "CreateFile (create)" ... / -Error while trying to create file ...

      Firebird *allows* to create database with path length = 259 but this DB can not be referred
      neither using rdb$get_context('SYSTEM','DB_NAME') nor vis MON$DATABASE.

      Firebird database with path+name length >= 253 can not be subject of NBACKUP -L because of ".delta" suffix
      which always is added to the full path+name of database and this will raise:
          PROBLEM ON "begin backup: commit".
          I/O error during "CreateFile (create)" operation for file "<...>.FDB.delta"
          -Error while trying to create file
          -<the system cannot find the path specified> // localized message here
          SQLCODE:-902

      Firebird database with length >= 246 can be backed up but can not be restored (if DB name will be such length).
      Maximal length of database that is created on Windows (<drive>:/<path>/<DB_filename>) is 245.

      Test uses this length (see below, MAX_FILE_NAME_SIZE) and cheks that one may to do following:
      * create such database and add some DB objects in it (table with index and procedure);
      * use encrypt and decrypt actions against this DB; SHOW database must display actual state of this DB;
      * extract metadata from this database;
      * call fb_lock_print utility with requirement to show Lock Manager info about this database;
      * invoke utilities: gstat -r; gfix; fbsvcmgr

      See also: http://tracker.firebirdsql.org/browse/CORE-6248

      All these actions are performed with active trace user session, which registers database and services activity.
      Finally we check that:
      * all above mentioned actions did not failed, i.e. they did not issue somewhat into STDERR;
      * trace log has records about all services which did start.
      * trace log does NOT contain 'FAILED START_SERVICE'

      Note-1.
      Test database that is created by fbtest framework will be encrypted here using IBSurgeon Demo Encryption package
      ( https://ib-aid.com/download-demo-firebird-encryption-plugin/ ; https://ib-aid.com/download/crypt/CryptTest.zip )
      License file plugins/dbcrypt.conf with unlimited expiration was provided by IBSurgeon to Firebird Foundation (FF).
      This file was preliminary stored in FF Test machine.
      Test assumes that this file and all neccessary libraries already were stored into FB_HOME and %FB_HOME%/plugins.

      Anyone who wants to run this test on his own machine must
      1) download https://ib-aid.com/download/crypt/CryptTest.zip AND
      2) PURCHASE LICENSE and get from IBSurgeon file plugins/dbcrypt.conf with apropriate expiration date and other info.

      ################################################ ! ! !    N O T E    ! ! ! ##############################################
      FF tests storage (aka "fbt-repo") does not (and will not) contain any license file for IBSurgeon Demo Encryption package!
      #########################################################################################################################

      Note-2.
      Encryption/decryption is executed as separate server thread and can not be done instantly thus requiring some pause.
      This delay is implemented by starting transaction with LOCK TIMEOUT <n> and making attempt to insert duplicate into
      table with unique index. Though error can be supressed in PSQL code, it *does* appear in the trace log as two lines:
          Error in the trace: 335544665 : violation of PRIMARY or UNIQUE KEY constraint "TEST_UNQ" on table "TEST"
          Error in the trace: 335545072 : Problematic key value is ("S" = '000000000000000000000000000000000000')
      We have to ignore these errors because it is expected (they must appear twise: after start of encryption and decruption).

      Checked on:
          4.0.0.1767 SS: 15.266s.
          4.0.0.1712 SC: 16.438s.
          4.0.0.1763 CS: 17.719s.
          3.0.6.33246 SS: 10.656s.
          3.0.5.33084 SC: 14.563s.
          3.0.6.33246 CS: 14.595s.

     21.01.2021: added check for trace STDERR because of crash FB 4.0.0.2335 SS/CS. Refactored code for saving logs.
FBTEST:      functional.tabloid.max_path_length
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

expected_stdout = """
    Expected error in the trace: 335544665 : violation of PRIMARY or UNIQUE KEY constraint "TEST_UNQ" on table "TEST"
    Expected error in the trace: 335545072 : Problematic key value is ("S" = '000000000000000000000000000000000000')
    Expected error in the trace: 335544665 : violation of PRIMARY or UNIQUE KEY constraint "TEST_UNQ" on table "TEST"
    Expected error in the trace: 335545072 : Problematic key value is ("S" = '000000000000000000000000000000000000')
    Pattern 1. DB_REPAIR : FOUND in the trace log
    Pattern 2. DB_VALIDATE : FOUND in the trace log
    Pattern 3. DB_PROPS : FOUND in the trace log
    Pattern 4. DB_BACKUP : FOUND in the trace log
    Pattern 5. DB_RESTORE : FOUND in the trace log
    Pattern 6. DB_NBACKUP : FOUND in the trace log
    Pattern 7. DB_NRESTORE : FOUND in the trace log
"""

@pytest.mark.skip('FIXME: Not IMPLEMENTED')
@pytest.mark.version('>=3.0')
@pytest.mark.platform('Windows')
def test_1(act: Action):
    pytest.fail("Not IMPLEMENTED")

# Original python code for this test:
# -----------------------------------
#
# import os
# import sys
# import time
# import re
# import subprocess
# from fdb import services
#
# MAX_FILE_NAME_SIZE=245 # last value with result = ok
# #MAX_FILE_NAME_SIZE=246 # 246 --> get error in gbak; 254 --> can not run nbackup -L
# #MAX_FILE_NAME_SIZE=254
#
# os.environ["ISC_USER"] = user_name
# os.environ["ISC_PASSWORD"] = user_password
#
# db_conn.close()
#
# #--------------------------------------------
#
# def flush_and_close(file_handle):
#     # https://docs.python.org/2/library/os.html#os.fsync
#     # If you're starting with a Python file object f,
#     # first do f.flush(), and
#     # then do os.fsync(f.fileno()), to ensure that all internal buffers associated with f are written to disk.
#     global os
#
#     file_handle.flush()
#     if file_handle.mode not in ('r', 'rb'):
#         # otherwise: "OSError: [Errno 9] Bad file descriptor"!
#         os.fsync(file_handle.fileno())
#     file_handle.close()
#
# #--------------------------------------------
#
# def cleanup( f_names_list ):
#     global os
#     for i in range(len( f_names_list )):
#        if os.path.isfile( f_names_list[i]):
#             os.remove( f_names_list[i] )
#
# #--------------------------------------------
#
#
# fb_home = services.connect(host='localhost', user= user_name, password= user_password).get_home_directory()
#
# db_folder=context['temp_directory']
# db_ext = '.fdb'
# bk_ext = '.fbk'
# db_longest_name = ('1234567890' * 1000) [ : MAX_FILE_NAME_SIZE - len( db_folder ) - len(db_ext) ] + db_ext
# bk_longest_name = db_longest_name[ : -len(db_ext )] + bk_ext
# db_longest_repl = db_longest_name[ : -len(db_ext )] + '.tmp'
# nb_longest_name = db_longest_name[ : -len(db_ext )] + '.nb0'
#
# # --------------------------- generate trace config and launch trace ------------------------------------
#
# txt = '''    database=%%[\\\\\\\\/]%(db_longest_name)s
#     {
#         enabled = true
#         time_threshold = 0
#         log_initfini = false
#         log_connections = true
#         # log_transactions = true
#         log_errors = true
#         log_sweep = true
#         log_statement_finish = true
#     }
#
#     services
#     {
#         enabled = true
#         log_errors = true
#         log_initfini = false
#         log_services =  true
#         exclude_filter = "%%((List Trace Sessions)|(Start Trace Session)|(Stop Trace Session))%%"
#         # include_filter = "%%((backup database)|(restore database)|(repair database)|(validate database)|(incremental backup database)|(incremental restore database))%%"
#     }
# ''' % locals()
#
# trc_cfg=open( os.path.join(context['temp_directory'],'trc_maxpath.cfg'), 'w')
# trc_cfg.write(txt)
# trc_cfg.close()
#
# #####################################################################
# # Async. launch of trace session using FBSVCMGR action_trace_start:
#
# trc_log = open( os.path.join(context['temp_directory'],'trc_maxpath.log'), "w")
# trc_err = open( os.path.join(context['temp_directory'],'trc_maxpath.err'), "w")
# # Execute a child program in a new process, redirecting STDERR to the same target as of STDOUT:
# p_svcmgr = subprocess.Popen( [ "fbsvcmgr", "localhost:service_mgr",
#                     "action_trace_start",
#                     "trc_cfg", trc_cfg.name
#                   ],
#                   stdout=trc_log,
#                   stderr=trc_err
#                 )
#
# # 08.01.2020. This delay is mandatory, otherwise file with trace session info can remain (sometimes)
# # empty when we will read it at the next step:
# time.sleep(2)
#
#
# # Determine active trace session ID (for further stop):
# ########################
# trc_lst=open( os.path.join(context['temp_directory'],'trc_maxpath.lst'), 'w')
# subprocess.call(["fbsvcmgr", "localhost:service_mgr",
#                  "action_trace_list"],
#                  stdout=trc_lst, stderr=subprocess.STDOUT
#                )
# flush_and_close( trc_lst )
#
# # Session ID: 5
# #   user:
# #   date:  2015-08-27 15:24:14
# #   flags: active, trace
#
# sid_pattern = re.compile('Session\\s+ID[:]{0,1}\\s+\\d+', re.IGNORECASE)
#
# trc_ssn=0
# with open( trc_lst.name,'r') as f:
#     for line in f:
#         if sid_pattern.search( line ) and len( line.split() ) == 3:
#             trc_ssn = line.split()[2]
#             break
#
# # Result: `trc_ssn` is ID of active trace session.
# # We have to terminate trace session that is running on server BEFORE we termitane process `p_svcmgr`
#
#
# # ---------------------- generating .SQL and create databse with long name ------------------------------
#
# if os.path.isfile( db_folder + db_longest_name ):
#     os.remove( db_folder + db_longest_name )
#
# sql_ddl='''
#     -- set bail on;
#     set list on;
#     set names utf8;
#     shell del %(db_folder)s%(db_longest_name)s 2>nul;
#     create database 'localhost:%(db_folder)s%(db_longest_name)s' default character set utf8;
#     create table test( s varchar(36), constraint test_unq unique(s) );
#     commit;
#     insert into test(s) values( lpad('', 36, '0') );
#     commit;
#
#     alter database set linger to 0;
#     commit;
#     insert into test select uuid_to_char(gen_uuid()) from rdb$types;
#     commit;
#     set term ^;
#     create procedure sp_pause as
#         declare s1 varchar(36);
#     begin
#         update test set s = s
#         order by s rows 1
#         returning s into s1;
#         execute statement ( 'insert into test(s) values(?)' ) ( s1 )
#         on external
#             'localhost:' || rdb$get_context('SYSTEM','DB_NAME')
#             as user 'SYSDBA' password 'masterkey' role left(replace( uuid_to_char(gen_uuid()), '-', ''), 31);
#         when any do
#             begin
#
#             end
#     end
#     ^
#     set term ;^
#     commit;
#
#     alter database encrypt with dbcrypt key Red;
#     commit;
#     set transaction lock timeout 1;
#     execute procedure sp_pause;
#     show database;
#
#     alter database decrypt;
#     commit;
#     set transaction lock timeout 1;
#     execute procedure sp_pause;
#     show database;
#
#     shell %(fb_home)sfb_lock_print -c -d %(db_folder)s%(db_longest_name)s;
#
# ''' % locals()
#
# f_isql_cmd=open( os.path.join(context['temp_directory'],'tmp_maxpath_test.sql'), 'w', buffering = 0)
# f_isql_cmd.write(sql_ddl)
# flush_and_close( f_isql_cmd )
#
# # ---------------------- generating .bat for execute actions with DB via Firebird utilities -------------------------
#
# f_isql_name=f_isql_cmd.name
# tmp_bat_text='''
#     @echo off
#     setlocal enabledelayedexpansion enableextensions
#     set tmplog=%%~dpn0.log
#     set tmperr=%%~dpn0.err
#
#     del !tmplog! 2>nul
#     del !tmperr! 2>nul
#
#     set ISC_USER=%(user_name)s
#     set ISC_PASSWORD=%(user_password)s
#
#     set run_cmd=%(fb_home)sisql -q -i %(f_isql_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)sisql localhost:%(db_folder)s%(db_longest_name)s -x -ch utf8
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)sgstat -r localhost:%(db_folder)s%(db_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)sgfix -shut full -force 0 localhost:%(db_folder)s%(db_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)sgfix -online localhost:%(db_folder)s%(db_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)sgfix -v -full localhost:%(db_folder)s%(db_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)sgfix -w async localhost:%(db_folder)s%(db_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     @rem ---------------------------------------------------------------------------------------
#
#
#     set run_cmd=%(fb_home)sgfix -sweep localhost:%(db_folder)s%(db_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)snbackup -L %(db_folder)s%(db_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)snbackup -N %(db_folder)s%(db_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)sfbsvcmgr localhost:service_mgr action_repair rpr_sweep_db dbname %(db_folder)s%(db_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#
#     @rem ---------------------------------------------------------------------------------------
#
#
#     set run_cmd=%(fb_home)sfbsvcmgr localhost:service_mgr action_validate dbname %(db_folder)s%(db_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)sfbsvcmgr localhost:service_mgr action_properties dbname %(db_folder)s%(db_longest_name)s prp_sweep_interval 12345
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)sfbsvcmgr localhost:service_mgr action_backup dbname %(db_folder)s%(db_longest_name)s bkp_file %(db_folder)s%(bk_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#
#     if exist %(db_folder)s%(bk_longest_name)s (
#         set run_cmd=%(fb_home)sfbsvcmgr localhost:service_mgr action_restore res_replace bkp_file %(db_folder)s%(bk_longest_name)s dbname %(db_folder)s%(db_longest_repl)s
#         echo. >>!tmplog!
#         echo !run_cmd! 1>>!tmplog!
#         cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     ) else (
#         (
#             echo Record-level backup via FB services API FAILED, target file:
#             echo %(db_folder)s%(bk_longest_name)s
#             echo -- does not exist.
#             echo Subsequent restore from this file has been SKIPPED.
#         ) >>!tmplog!
#     )
#
#     del %(db_folder)s%(bk_longest_name)s 2>nul
#
#
#     @rem ---------------------------------------------------------------------------------------
#
#     @rem Drop TARGET file for ongoing INCREMENTAL BACKUP operation:
#     @rem ~~~~~~~~~~~~~~~~
#     del %(db_folder)s%(nb_longest_name)s 2>nul
#
#     set run_cmd=%(fb_home)sfbsvcmgr localhost:service_mgr action_nbak dbname %(db_folder)s%(db_longest_name)s nbk_file %(db_folder)s%(nb_longest_name)s nbk_level 0
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#
#     @rem ---------------------------------------------------------------------------------------
#
#     if exist %(db_folder)s%(nb_longest_name)s (
#
#         @rem Drop TARGET file for ongoing INCREMENTAL RESTORE operation:
#         @rem ~~~~~~~~~~~~~~~~
#         del %(db_folder)s%(db_longest_repl)s 2>nul
#
#         set run_cmd=%(fb_home)sfbsvcmgr localhost:service_mgr action_nrest nbk_file %(db_folder)s%(nb_longest_name)s dbname %(db_folder)s%(db_longest_repl)s
#         echo. >>!tmplog!
#         echo !run_cmd! 1>>!tmplog!
#         cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     ) else (
#         (
#             echo Physical-level backup via FB services API FAILED, target file:
#             echo %(db_folder)s%(nb_longest_name)s
#             echo -- does not exist.
#             echo Subsequent incremental restore from this file has been SKIPPED.
#         ) >>!tmplog!
#     )
#
#
#     @rem ---------------------------------------------------------------------------------------
#
#     set run_cmd=%(fb_home)sgbak -b -v localhost:%(db_folder)s%(db_longest_name)s %(db_folder)s%(bk_longest_name)s
#     echo. >>!tmplog!
#     echo !run_cmd! 1>>!tmplog!
#     cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#     @rem ---------------------------------------------------------------------------------------
#
#     if exist %(db_folder)s%(bk_longest_name)s (
#
#         (
#             echo.
#             echo Check backup file from which we will perform record-level restore:
#             dir /-c %(db_folder)s%(bk_longest_name)s | findstr /i /c:"%(bk_ext)s"
#             echo.
#         ) 1>>!tmplog!
#
#         set run_cmd=%(fb_home)sgbak -rep -v %(db_folder)s%(bk_longest_name)s localhost:%(db_folder)s%(db_longest_repl)s
#         echo. >>!tmplog!
#         echo !run_cmd! 1>>!tmplog!
#         cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#         @rem ---------------------------------------------------------------------------------------
#
#         @rem set run_cmd=%(fb_home)sgbak -b localhost:%(db_folder)s%(db_longest_name)s stdout ^| %(fb_home)sgbak -rep -v stdin localhost:%(db_folder)s%(db_longest_repl)s
#         @rem echo. >>!tmplog!
#         @rem echo !run_cmd! 1>>!tmplog!
#         @rem cmd /c !run_cmd! 1>>!tmplog! 2>!tmperr!
#         @rem ---------------------------------------------------------------------------------------
#     ) else (
#
#         (
#             echo Record-level backup using gbak utility FAILED, target file:
#             echo %(db_folder)s%(bk_longest_name)s
#             echo -- does not exist.
#             echo Subsequent restore from this file has been SKIPPED.
#         ) >>!tmplog!
#     )
#
#     del %(db_folder)s%(nb_longest_name)s 2>nul
#     del %(db_folder)s%(bk_longest_name)s 2>nul
#     del %(db_folder)s%(db_longest_name)s 2>nul
#     del %(db_folder)s%(db_longest_repl)s 2>nul
#
# ''' % dict(globals(), **locals())
#
#
# # ---------------------- execute .bat  -------------------------
#
# f_tmp_bat=open( os.path.join(context['temp_directory'],'tmp_maxpath_test.bat'), 'w', buffering = 0)
# f_tmp_bat.write( tmp_bat_text )
# flush_and_close( f_tmp_bat )
#
# subprocess.call( [ f_tmp_bat.name ] )
#
# # ::: NB ::: Here we have to be idle at least 2s (two seconds) otherwise trace log will
# # not contain some or all of messages about create DB, start Tx, ES, Tx and drop DB.
# # See also discussion with hvlad, 08.01.2020 15:16
# # (subj: "action_trace_stop does not flush trace log (fully or partially)")
# time.sleep(2)
#
# # Stop trace session:
# #####################
#
# trc_lst=open(trc_lst.name, "a")
# trc_lst.seek(0,2)
#
# subprocess.call( [ "fbsvcmgr", "localhost:service_mgr",
#                    "action_trace_stop",
#                    "trc_id",trc_ssn
#                  ],
#                  stdout=trc_lst,
#                  stderr=subprocess.STDOUT
#                )
# flush_and_close( trc_lst )
#
# p_svcmgr.terminate()
# flush_and_close( trc_log )
# flush_and_close( trc_err )
#
# f_bat_log = '.'.join( ( os.path.splitext( f_tmp_bat.name )[0], 'log') )
# f_bat_err = '.'.join( ( os.path.splitext( f_tmp_bat.name )[0], 'err') )
#
# with open(f_bat_err,'r', buffering = 0) as f:
#     for line in f:
#         if line.split():
#             print( 'Unexpected STDERR: ' + line )
#
#
# runtime_error_ptn = re.compile( '\\d{8}\\s+:\\s+.' )
# services_patterns = {
#      '1. DB_REPAIR'   : re.compile('"Repair\\s+Database"', re.IGNORECASE)
#     ,'2. DB_VALIDATE' : re.compile('"Validate\\s+Database"', re.IGNORECASE)
#     ,'3. DB_PROPS'    : re.compile('"Database\\s+Properties"', re.IGNORECASE)
#     ,'4. DB_BACKUP'   : re.compile('"Backup\\s+Database"', re.IGNORECASE)
#     ,'5. DB_RESTORE'  : re.compile('"Restore\\s+Database"', re.IGNORECASE)
#     ,'6. DB_NBACKUP'  : re.compile('"Incremental\\s+Backup\\s+Database"', re.IGNORECASE)
#     ,'7. DB_NRESTORE' : re.compile('"Incremental\\s+Restore\\s+Database"', re.IGNORECASE)
# }
#
# found_patterns={}
#
# with open( trc_err.name,'r') as f:
#     for line in f:
#         if line.rstrip():
#             print( 'UNEXPECTED error in the trace: ' + line )
#
# with open( trc_log.name,'r') as f:
#     for line in f:
#         if line.rstrip().split():
#             if 'FAILED START_SERVICE' in line:
#                 print( 'UNEXPECTED error with FB SERVICES: ' + line )
#             if runtime_error_ptn.search(line):
#                 if '335544528 :'  in line:
#                     # ::: NOTE :::
#                     # Change DB state to full shutdown (by gfix) produced in the trace log
#                     # failed_attach event that is issued by secondary attachment of gfix.exe:
#                     #     FAILED ATTACH_DATABASE
#                     #     <drive:path	o\\gfix.exe>
#                     #     ERROR AT JProvider::attachDatabase
#                     #     <drive:path	o\\gfix.exe>
#                     #     335544528 : database ... shutdown
#                     # This is actutal at least for Firebird 3.0.6, so we have to IGNORE this line.
#                     pass
#                 else:
#                     print( 'Expected error in the trace: ' + line )
#
#             for k,v in services_patterns.items():
#                 if v.search(line):
#                     found_patterns[k] = 'FOUND in the trace log'
#
#
# for k,v in sorted( found_patterns.items() ):
#     print( 'Pattern', k, ':', v)
#
# ################################################
#
# # 02.04.2020, WindowsError: 32 The process cannot access the file because it is being used by another process
# #############
# time.sleep(2)
#
# #cleanup:
# f_list = [ i.name for i in (trc_cfg, trc_log, trc_err, trc_lst, f_isql_cmd, f_tmp_bat) ]
# f_list += [ db_longest_name,bk_longest_name,db_longest_repl,nb_longest_name, f_bat_log, f_bat_err ]
#
# cleanup( f_list )
#
# -----------------------------------
