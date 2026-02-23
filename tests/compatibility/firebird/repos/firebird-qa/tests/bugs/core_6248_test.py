#coding:utf-8

"""
ID:          issue-6492
ISSUE:       6492
TITLE:       A number of errors when database name is longer than 255 symbols
DESCRIPTION:
    From ticket title one may to conclude that we have ability to really create databases with full length (path + file name)
    more than 255 characters, but it is not so.
    Actually, on Windows and Linux one can not create files with length more than 255 characters.
    Moreover, FB has its own, more strict limitation: command CONNECT '<database_name>' can not operate with files with length
    more than (253 - <P>) symbols, including enclosing quotes, where <P> is length of used protocol prefix: 'inet://', 'localhost:'.

    Test verifies ability to create DB with max possible length (i.e. which allows to run 'CONNECT ...' command) and does that
    for all protocols that are supported by current OS (Linux + local, inet;  Windows: local, inet, xnet).
    For each used protocol we try to perform several operations using gfix utility and after this - fbsvcmgr.

    After each operation we check mon$database and 'construct' string based on values from this table that must be returned
    as tail of 'Attributes' line in the 'gstat -h' command. These lines must be equal.
    One need to pay attantion that when database is in 'backup-lock' state then changes in its attributes can not be seen
    in the output of 'gstat -h'. Because of this, we do not change any attributes between 'alter database begin/end backup'.
    Also, one need to take in account that we can not check full ability of 'nbackup -L' command for databases which names
    are maximal possible, because adding of suffix '.delta' will cause exceeding max possible length = 253 characters.
    Because of this, test uses 'alter database begin add difference file with name = '<database_path_name.dif>' (i.e. its has
    the same length and main DB file).

NOTES:
    [09.03.2020] pzotov // old comments, to be deleted later.
        STDOUT-logs of backup, restore and gstat currently (09-mar-2020) have only truncated name (~235...241 chars).
        This may change in the future if FB developers will decide to resolve this problem.

        For L=259 we must see in backup log following phrase:
          gbak:text for attribute 7 is too large in put_asciz(), truncating to 255 bytes
        - but currently this is not checked here.
    [09.02.2022] pcisar
        Fails on Windows10 / 4.0.1 with:
         "CreateFile (create)" operation for file "..."
          -Error while trying to create file
          -System can't find specified path
        Variant with 255 chars fails in init script, while 259 chars variant fails in database fixture while
        db creation.
        On national windows with OS i/o error messages in locale.getpreferredencoding(), it may fail while
        reading stderr from isql. But using io_enc=locale.getpreferredencoding() will show the message.
    [23.09.2022] pzotov
        1. Database header (that is produced by 'gstat -h' command) contains in its 1st line full path + name of DB file,
           enclosed into double quotes. When length of path+filename is 243 or greater, this string begins look as 'cuted',
           i.e. ellipsis will be shown at the end of this name (instead of extension and closed double quote).
           This means that we have to be aware about applying regexp during parsing gstat output: final quote may miss!
        2. Currently one can not use 'act.svccmgr()' calls because of need to specify different protocols when check fbsvcmgr:
               fbsvcmgr service_mgr ...
               fbsvcmgr inet://localhost:service_mgr ...
               fbsvcmgr xnet://service_mgr ...
           Because of this, subprocess.run() is used to invoke fbsvcmgr

    Checked on Windows and Linux, 4.0.1.2692 (SS/CS), 5.0.0.736 (SS/CS).

    [25.02.2025] pzotov
        Order of attributes changed since 93db88 ("ODS14: header page refactoring (#8401)"):
        mon_read_only is displayed now PRIOR to mon_shutdown_mode.
        Re-implmented code for building 'Attributes' line using mon$database values, see func check_db_hdr_info.
        Checked on Windows, 6.0.0.652, 5.0.3.1622

JIRA:        CORE-6248
FBTEST:      bugs.core_6248
"""
import os
import subprocess
import locale
import re
import time
import platform
from pathlib import Path
#from difflib import unified_diff

from firebird.driver import connect

import pytest
from firebird.qa import *

db = db_factory()
act = python_act('db', substitutions = [('[\t ]+', ' ')])

# We have to limit length of temp_file with 255 characters.
# CentOS-7: OSError: [Errno 36] File name too long:
tmp_file = temp_file( ('0123456789' * 26)[:255] )

#-----------------------------------------------
def check_db_hdr_info(act: Action, db_file_chk:Path, interested_patterns, capsys):

    # 1. Obtain attributes from mon$database: get page buffers, 'build' attributes row and get sweep interval.
    #    These values will be displayed in the form of three separate LINES, without column names.
    #    Content of this output must be equal to gstat filtered values, with exception of leading spaces:

    expected_attr_from_mon_db = ''
    with connect(str(db_file_chk), user = act.db.user) as con:
        cur = con.cursor()
        sql = """
            select
                rdb$get_context('SYSTEM', 'ENGINE_VERSION') as engine_ver
               ,mon$page_buffers as mon_page_buffers
               ,mon$sweep_interval as mon_sweep_interval
               ,trim(iif(mon$forced_writes = 1, ', force write', '')) as mon_force_write
               ,trim(iif(mon$reserve_space = 0, ', no reserve', '')) as mon_reserve_space
               ,trim(decode(mon$backup_state,  2, ', merge', 1, ', backup lock', '')) as mon_backup_state
               ,trim(decode(mon$shutdown_mode, 3, 'full shutdown', 2,', single-user maintenance', 1,', multi-user maintenance',  '')) as mon_shutdown_mode
                -- ::: NEED TRIM() ::: otherwise 10 spaces will be inserted if mon$read_only=0.
                -- Discussed with Vlad et al, letters since 23.09.2022 10:57.
               ,trim(iif(mon$read_only = 1, ', read only', '')) as mon_read_only
               ,trim(decode(mon$replica_mode, 2,', read-write replica', 1,', read-only replica',  '')) as mon_replica_mode
            from mon$database
        """
        cur.execute(sql)
        for r in cur:
            engine_ver, mon_page_buffers, mon_sweep_interval, mon_force_write, mon_reserve_space, mon_backup_state, mon_shutdown_mode, mon_read_only, mon_replica_mode = r[:9]

            expected_attr_from_mon_db += f'Page buffers {mon_page_buffers}'
            if act.is_version('<6'):
                attr_list = f'{mon_force_write}{mon_reserve_space}{mon_backup_state}{mon_shutdown_mode}{mon_read_only}{mon_replica_mode}'
            else:
                ### ACHTUNG, SINCE 6.0.0.647 2025.02.21 ###
                # Order of attributes has changed since #93db88 ("ODS14: header page refactoring (#8401)"), 20-feb-2025:
                # mon_read_only is displayed now PRIOR to mon_shutdown_mode:
                attr_list = f'{mon_force_write}{mon_reserve_space}{mon_backup_state}{mon_read_only}{mon_shutdown_mode}{mon_replica_mode}'

            expected_attr_from_mon_db += f'\nAttributes {attr_list[2:]}'
            expected_attr_from_mon_db += f'\nSweep interval: {mon_sweep_interval}'
    
    #------------------------------------------------------

    # 2. Run 'gstat -h', filtering its output and compare with data that was obtained from mon$database.
    #    NOTE: database name with backslashes (on Windows) must be checked without regexp work, only via 'IN'.
    #    On Winows databases are created in UPPER form, so we have to remove case sensitivity.
    act.gstat(switches=['-h', db_file_chk], connect_db = False, io_enc = locale.getpreferredencoding())
    db_guid = ''
    db_found = ''
    db_cuted = ('Database "' + str(db_file_chk) + '"').lower()[:250]
    for line in act.stdout.split('\n'):
        if act.match_any(line, interested_patterns):
            if 'Database GUID' in line:
                db_guid = line
            elif db_cuted in line.lower():
                print(db_cuted)
                db_found = 1
            else:
                print(line)

    if db_found:
        act.expected_stdout = f"""
            {db_cuted}
            {expected_attr_from_mon_db}
        """        
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()
    else:
        print('Cuted DB name:',db_cuted)
        for line in act.stdout.split('\n'):
            print('gstat output: ',line)
        assert db_found,'COULD NOT FIND NAME OF DATABASE IN THE GSTAT HEADER'

    # 3. Return GUID of database (can be compared after b/r with GUID of restored database: they always must differ):
    return db_guid

#-----------------------------------------------
@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_file:Path, capsys):

    #################
    # ### ACHTUNG ###
    #################
    # DO NOT include ending double quote into the database name pattern!
    # String with database name is CUTED OFF when length of full path + filename is 243 and above:
    # 243 --> Database "C:\TEMP\...\01234567890<...>012345.F...
    # 244 --> Database "C:\TEMP\...\01234567890<...>0123456....
    # 245 --> Database "C:\TEMP\...\01234567890<...>01234567...
    # 246 ... 255 -- same as for 245. NB: for N=244 line differs from all others.
    # All these (cuted) strings have length = 254 bytes and do NOT contain ending double quote.
    # Because of this, we must include this character into the pattern only as OPTIONAL, i.e.: |'Database\s+"\S+(")?'|
    #
    interested_patterns = ( r'Database\s+"\S+(")?', r'[\t ]*Attributes([\t ]+\w+)?', r'[\t ]*Page buffers([\t ]+\d+)', r'[\t ]*Sweep interval(:)?([\t ]+\d+)', 'Database GUID')
    interested_patterns = [re.compile(p, re.IGNORECASE) for p in interested_patterns]
    protocol_list = ('', 'inet://', 'xnet://') if os.name == 'nt' else ('', 'inet://',)
    
    full_str = str(tmp_file.absolute())

    for chk_mode in ('fb_util', 'fbsvcmgr'):
        for protocol_prefix in protocol_list:

            # NB: most strict limit for DB filename length origins from isql 'CONNECT' command:
            # 'command error' raises there if length of '{db_file_chk}' including qutes greater than 255.
            # Because of this, we can not operate with files with length of full name greater than 253 bytes.
            #
            db_file_len = 253 - len(protocol_prefix)

            db_file_chk = Path((full_str[:db_file_len-4] + '.fdb').lower())
            db_file_dif = Path(os.path.splitext(db_file_chk)[0] + '.dif')
            db_file_fbk = Path(os.path.splitext(db_file_chk)[0] + '.fbk')

            db_file_dif.unlink(missing_ok = True)

            db_file_dsn = ''
            svc_call_starting_part = []
            if chk_mode == 'fb_util':
                db_file_dsn = protocol_prefix + str(db_file_chk)
            else:
                db_file_dsn = db_file_chk
                fb_svc_name = protocol_prefix + 'service_mgr'
                svc_call_starting_part = [ act.vars['fbsvcmgr'], fb_svc_name, '-user', act.db.user, '-password', act.db.password ]

            sql_txt = f"""
                set list on;
                create database '{db_file_dsn}' user {act.db.user} password '{act.db.password}';
                select lower(mon$database_name) as mon_db_name from mon$database;
                select lower(rdb$get_context('SYSTEM', 'DB_NAME')) as ctx_db_name from mon$database;
                commit;
            """

            act.expected_stdout = f"""
                MON_DB_NAME {db_file_chk}
                CTX_DB_NAME {db_file_chk}
            """

            act.isql(switches = ['-q'], input = sql_txt, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()

            svc_retcode = 0
            if chk_mode == 'fb_util':
                act.gfix(switches=['-buffers', '3791', db_file_dsn], combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                svc_retcode = (subprocess.run( svc_call_starting_part + ['action_properties', 'prp_page_buffers', '3791', 'dbname', db_file_chk], stderr = subprocess.STDOUT)).returncode
            
            assert '' == act.stdout and svc_retcode == 0
            act.reset()


            if chk_mode == 'fb_util':
                act.gfix(switches=['-write','sync', db_file_dsn], combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                svc_retcode = (subprocess.run( svc_call_starting_part + ['action_properties', 'prp_write_mode', 'prp_wm_sync', 'dbname', db_file_chk], stderr = subprocess.STDOUT)).returncode

            assert '' == act.stdout and svc_retcode == 0
            act.reset()


            if chk_mode == 'fb_util':
                act.gfix(switches=['-housekeeping','5678', db_file_dsn], combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                svc_retcode = (subprocess.run( svc_call_starting_part + ['action_properties', 'prp_sweep_interval', '5678', 'dbname', db_file_chk], stderr = subprocess.STDOUT)).returncode

            assert '' == act.stdout and svc_retcode == 0
            act.reset()


            if chk_mode == 'fb_util':
                act.gfix(switches=['-use','full', db_file_dsn], combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                svc_retcode = (subprocess.run( svc_call_starting_part + ['action_properties', 'prp_reserve_space', 'prp_res_use_full', 'dbname', db_file_chk], stderr = subprocess.STDOUT)).returncode
            
            assert '' == act.stdout and svc_retcode == 0
            act.reset()

            
            if chk_mode == 'fb_util':
                act.gfix(switches=['-sweep', db_file_dsn], combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                svc_retcode = (subprocess.run( svc_call_starting_part + ['action_repair', 'rpr_sweep_db', 'dbname', db_file_chk], stderr = subprocess.STDOUT)).returncode
            
            assert '' == act.stdout and svc_retcode == 0
            act.reset()

            if act.is_version('>=4'):
                if chk_mode == 'fb_util':
                    act.gfix(switches=['-replica','read_write', db_file_dsn], combine_output = True, io_enc = locale.getpreferredencoding())
                else:
                    svc_retcode = (subprocess.run( svc_call_starting_part + ['action_properties', 'prp_replica_mode', 'prp_rm_readwrite', 'dbname', db_file_chk], stderr = subprocess.STDOUT)).returncode

                assert '' == act.stdout and svc_retcode == 0
                act.reset()


            sql_txt = f"""
                -- Make connect using local protocol.
                -- NOTE: 'command error' raises here if length of '{db_file_chk}' including qutes greater than 255.
                connect '{db_file_chk}' user {act.db.user};
                alter database add difference file '{db_file_dif}';
                alter database begin backup;
                -- alter database set linger to 100;
            """
            
            # Page buffers 3791
            # Attributes  force write, no reserve, backup lock, read-write replica
            # Sweep interval: 5678
            #
            act.isql(switches = ['-q'], input = sql_txt, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
            assert act.clean_stdout == act.clean_expected_stdout
            
            # WRONG for 4.x ... 5.x: assert '' == act.stdout -- noise characters are in output:
            # "SQL> SQL> SQL> SQL> Database: ..."

            act.reset()
            _ = check_db_hdr_info(act, db_file_chk, interested_patterns, capsys)

            sql_txt = f"""
                -- Make connect using local protocol.
                -- NOTE: 'command error' raises here if length of '{db_file_chk}' including qutes greater than 255.
                connect '{db_file_chk}' user {act.db.user};
                -- alter database set linger to 0;
                alter database end backup;
            """
            act.isql(switches = ['-q'], input = sql_txt, connect_db=False, credentials = False, combine_output = True, io_enc = locale.getpreferredencoding())
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()

            if chk_mode == 'fb_util':
                act.gfix(switches=['-mode','read_only', db_file_dsn], combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                svc_retcode = (subprocess.run( svc_call_starting_part + ['action_properties', 'prp_access_mode', 'prp_am_readonly', 'dbname', db_file_chk], stderr = subprocess.STDOUT)).returncode

            assert '' == act.stdout and svc_retcode == 0
            act.reset()
            _ = check_db_hdr_info(act, db_file_chk, interested_patterns, capsys)


            if chk_mode == 'fb_util':
                act.gfix(switches=['-shut','single', '-at', '20', db_file_dsn], combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                svc_retcode = (subprocess.run( svc_call_starting_part + ['action_properties', 'prp_shutdown_mode', 'prp_sm_single', 'prp_deny_new_attachments', '20', 'dbname', db_file_chk], stderr = subprocess.STDOUT)).returncode

            assert '' == act.stdout and svc_retcode == 0
            act.reset()

            src_guid = check_db_hdr_info(act, db_file_chk, interested_patterns, capsys)


            if chk_mode == 'fb_util':
                act.gfix(switches=['-online', db_file_dsn], combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                svc_retcode = (subprocess.run( svc_call_starting_part + ['action_properties', 'prp_online_mode', 'prp_sm_normal', 'dbname', db_file_chk], stderr = subprocess.STDOUT)).returncode

            assert '' == act.stdout and svc_retcode == 0
            act.reset()


            if chk_mode == 'fb_util':
                act.gfix(switches=['-v', '-full', db_file_dsn], combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                svc_retcode = (subprocess.run( svc_call_starting_part + ['action_repair', 'rpr_validate_db', 'rpr_full', 'dbname', db_file_chk], stderr = subprocess.STDOUT)).returncode

            assert '' == act.stdout and svc_retcode == 0
            act.reset()


            if chk_mode == 'fb_util':
                act.gbak(switches=['-b', db_file_dsn, db_file_fbk], combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                svc_retcode = (subprocess.run( svc_call_starting_part + ['action_backup', 'dbname', db_file_chk, 'bkp_file', db_file_fbk], stderr = subprocess.STDOUT)).returncode

            assert '' == act.stdout and svc_retcode == 0
            act.reset()


            if chk_mode == 'fb_util':
                act.gbak(switches=['-rep', db_file_fbk, db_file_dsn], combine_output = True, io_enc = locale.getpreferredencoding())
            else:
                svc_retcode = (subprocess.run( svc_call_starting_part + ['action_restore', 'dbname', db_file_chk, 'bkp_file', db_file_fbk, 'res_replace' ], stderr = subprocess.STDOUT)).returncode

            assert '' == act.stdout and svc_retcode == 0
            act.reset()

            new_guid = check_db_hdr_info(act, db_file_chk, interested_patterns, capsys)


            print('GUID changed ? ==> ', src_guid != new_guid)
            act.expected_stdout = """
                GUID changed ? ==>      True
            """        
            act.stdout = capsys.readouterr().out
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()

            for f in (db_file_chk,db_file_dif,db_file_fbk):
                f.unlink(missing_ok=True)

