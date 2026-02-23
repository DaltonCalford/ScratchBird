#coding:utf-8

"""
ID:          issue-5321
ISSUE:       5321
TITLE:       At least 5 seconds delay on disconnect could happen if disconnect happens close after Event Manager initialization
DESCRIPTION:
    This test uses Python multiprocessing package in order to spawn multiple processes with trivial job: attach/detach from DB.
    Number processes ('planned_attachments') and of iterations for each of them ('reconnect_count') depends on FB architecture
    which is obtained by call of SP sys_get_fb_arch.
    For ClassicServer weird effect was detected: when number of processes is more then ~34-35 some of Python processes may not
    finish (but CS processes are gone and thus database has no attachments).
    Database is created with DB_level trigger 'on disconnect' which issues post_event and writes 'now'-timestamp to the table.
    Because we have dozen of different running processes, one need to distinguish records in the table for every of attachments.
    But values of current_connection are not suitable: they are unknown outside of DB connection, i.e. at the moment when we
    just return from DB attachment we still need to know some number that was 'linked' to just finished connection process.
    For this purpose ROLES are used in this test: we generate them beforehand (see 'init_test' section) and when create new
    attachment then specify ROLE in its arguments: 'R_000' for 1st spawned python process, 'R_001' for 2nd etc.
    Value of current_role will be written by DB-trigger in the table LOG4DETACH.
    When attachment finishes and control is returned to Python child process, we catch current timestamp again and save it
    value inside Python data structure 'sqllst' - common list.
    After some process will finish iterations, it will build SQL script for further applying to database and save it to the file
    with name that reflects to this process: 'tmp_5034_after_detach_NNN.log' whene NNN = '000', '001' etc.
    This file will contain INSERT command with values of timestamp that were on every iteration just after disconnect.
    After all processes will finish, we'll have completed set of such files and they will be "concatenated" together for applying
    at once by single call of ISQL.
    Finally, we can analyze values of datediff() between moments of 'just before' and 'just after' disconnects.
    In order to properly detect records that correspond to post-apply scripts (with INSERT statements) special integer field 'SEQ'
    is used: it will have even values for event when we are 'just before detach' (i.e. "inside" database connection) and odd values
    when we are 'just after' (i.e. when connection has been just closed). The view 'v_top10_slow' is used to display connects which
    were closed too slow.
    Normally, this view should return ONE record with text 'Detaches were fast'.
    Otherwise it will return concrete values of time that was spent on detach process, in milliseconds.
    #######################
    ###  A C H T U N G  ###
    #######################
    Following parameters: 'planned_attachments' and 'reconnect_count' affects on test result **VERY** strong, especially on Classic.
    You may need to change them if test results are unstable. Do NOT make value of 'planned_attachments' more than 33-34 on Classic!

    Successfully checked on build 32276, SS/SC/CS.
    Confirmed error on WI-V3.0.0.32134, Cs: "MonitoringData: Cannot initialize the shared memory region / sh_mem_length_mapped is 0"

    23.11.2016
    Increased threshold to 6000 ms instead of old 5000 ms -- see
    http://web.firebirdsql.org/download/prerelease/results/archive/3.0.2.32630/bugs.core_5034.html
JIRA:        CORE-5034
FBTEST:      bugs.core_5034
"""

import pytest
from firebird.qa import *

init_script = """
    set bail on;

    create or alter view v_top10_slow as select 1 x from rdb$database;
    commit;


    set term ^;
    create or alter trigger trg_disc active on disconnect position 0 as
    begin
    end
    ^

    create or alter procedure sys_get_fb_arch (
         a_connect_with_usr varchar(31) default 'SYSDBA'
        ,a_connect_with_pwd varchar(31) default 'masterkey'
    ) returns(
        fb_arch varchar(50)
    ) as
        declare cur_server_pid int;
        declare ext_server_pid int;
        declare att_protocol varchar(255);
        declare v_test_sttm varchar(255);
        declare v_fetches_beg bigint;
        declare v_fetches_end bigint;
    begin

        -- Aux SP for detect FB architecture.

        select a.mon$server_pid, a.mon$remote_protocol
        from mon$attachments a
        where a.mon$attachment_id = current_connection
        into cur_server_pid, att_protocol;

        if ( att_protocol is null ) then
            fb_arch = 'Embedded';
        else if ( upper(current_user) = upper('SYSDBA')
                  and rdb$get_context('SYSTEM','ENGINE_VERSION') NOT starting with '2.5'
                  and exists(select * from mon$attachments a
                             where a.mon$remote_protocol is null
                                   and upper(a.mon$user) in ( upper('Cache Writer'), upper('Garbage Collector'))
                            )
                ) then
            fb_arch = 'SuperServer';
        else
            begin
                v_test_sttm =
                    'select a.mon$server_pid + 0*(select 1 from rdb$database)'
                    ||' from mon$attachments a '
                    ||' where a.mon$attachment_id = current_connection';

                select i.mon$page_fetches
                from mon$io_stats i
                where i.mon$stat_group = 0  -- db_level
                into v_fetches_beg;

                execute statement v_test_sttm
                on external
                     'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
                as
                     user a_connect_with_usr
                     password a_connect_with_pwd
                     role left('R' || replace(uuid_to_char(gen_uuid()),'-',''),31)
                into ext_server_pid;

                in autonomous transaction do
                select i.mon$page_fetches
                from mon$io_stats i
                where i.mon$stat_group = 0  -- db_level
                into v_fetches_end;

                fb_arch = iif( cur_server_pid is distinct from ext_server_pid,
                               'Classic',
                               iif( v_fetches_beg is not distinct from v_fetches_end,
                                    'SuperClassic',
                                    'SuperServer'
                                  )
                             );
            end

        fb_arch = fb_arch || ' ' || rdb$get_context('SYSTEM','ENGINE_VERSION');

        suspend;

    end

    ^ -- sys_get_fb_arch
    set term ;^
    commit;

    recreate table log4detach(
      id int generated by default as identity constraint pk_log4detach primary key using index pk_log4detach
      ,rno varchar(31) default current_role -- for worker #0 --> 'R_000', #1 --> 'R_001' etc
      ,dts timestamp default 'now'
      ,seq int
      ,who varchar(31) default current_user
    );
    commit;

    set term ^;
    execute block as
        declare v_stt varchar(128);
        declare i smallint;
        declare n smallint = 500;
    begin

        rdb$set_context('USER_SESSION','INITIAL_DDL','1');
        i=0;
        while ( i < n) do
        begin
            v_stt = 'drop role R_' || lpad(i, 3, '0');
            begin
                execute statement v_stt;
                when any do begin end
            end

            v_stt = 'create role R_' || lpad(i, 3, '0');
            execute statement v_stt;

            v_stt = 'grant R_' || lpad(i, 3, '0') || ' to tmp$c5034';
            execute statement v_stt;

            i = i + 1;
        end

    end
    ^

    create or alter trigger trg_disc active on disconnect position 0 as
    begin

        POST_EVENT 'FOO';

        if ( current_user = 'TMP$C5034' and rdb$get_context('USER_SESSION','INITIAL_DDL') is null )
        then
            in autonomous transaction do
            insert into log4detach default values;
    end
    ^
    set term ;^
    commit;

    create or alter view v_top10_slow as
    select distinct msg
    from
    (
        select iif( detach_ms > max_detach_ms,
                    'Slow detaches > '|| max_detach_ms ||' ms detected: ' || detach_ms || ' ms, from ' || dts_before_detach || ' to '||dts_after_detach,
                    'All detaches not exceeded threshold'
                  ) as msg
        from (
            select
                 rno
                ,datediff(millisecond from min(t0) to min(t1)) as detach_ms
                ,min(t0) as dts_before_detach
                ,min(t1) as dts_after_detach
                ,6000 as max_detach_ms                -- changed 23.11.2016; was: 5000
            --                ^
            --                |
            --   ##############################
            --   #   t h r e s h o l d,   ms  #
            --   ##############################
            from (
                select
                    rno,dts
                    ,iif(mod(seq,2)=0, seq, seq-1) seq2
                    ,iif( mod(seq,2)=0, dts, null) t0
                    ,iif( mod(seq,2)=1, dts, null) t1
                from (
                    select rno, dts
                    ,seq
                    from log4detach d
                    where rno starting with 'R_'
                    order by rno,seq
                )
            )
            group by rno, seq2
        )
        where detach_ms >= 0
        order by detach_ms desc
        rows 10
    );
    commit;
"""

db = db_factory(init=init_script)

tmp_user = user_factory('db', name='tmp$c5034', password='123')

act = python_act('db')

expected_stdout = """
    All detaches not exceeded threshold
    Records affected: 1
"""

@pytest.mark.skip('FIXME: Not IMPLEMENTED')
@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):
    pytest.fail("Not IMPLEMENTED")

# test_script_1
#---
#
#  import os
#  import fdb
#  import time
#  import subprocess
#  from multiprocessing import Process
#  from fdb import services
#
#  os.environ["ISC_USER"] = user_name
#  os.environ["ISC_PASSWORD"] = user_password
#
#  # Get FB architecture:
#  xcur=db_conn.cursor()
#  xcur.execute("select fb_arch from sys_get_fb_arch;")
#
#  for r in xcur:
#      fb_arch = r[0].split()[0]
#
#  dbfile=db_conn.database_name
#  db_conn.close()
#
#  ISQL_BINARY = context['isql_path']
#  svc = services.connect(host='localhost')
#  FB_HOME = os.path.normpath( svc.get_home_directory() ) # 'c:\firebird\' --> 'c:\firebird' (i.e. remove trailing backslash if needed)
#  svc.close()
#
#  if os.name == 'nt':
#      # For Windows we assume that client library is always in FB_HOME dir:
#      FB_CLNT=os.path.join(FB_HOME, 'fbclient.dll')
#  else:
#      # For Linux client library will be searched in 'lib' subdirectory of FB_HOME:
#      # con=fdb.connect( dsn='localhost:employee', user='SYSDBA', password='masterkey', fb_library_name='/var/tmp/fb40tmp/lib/libfbclient.so')
#      FB_CLNT=os.path.join(FB_HOME, 'lib', 'libfbclient.so' )
#
#  FBT_TEMP_DIR = os.path.normpath(context['temp_directory'])
#
#  #--------------------------------------------
#
#  def flush_and_close(file_handle):
#      # https://docs.python.org/2/library/os.html#os.fsync
#      # If you're starting with a Python file object f,
#      # first do f.flush(), and
#      # then do os.fsync(f.fileno()), to ensure that all internal buffers associated with f are written to disk.
#      global os
#
#      file_handle.flush()
#      if file_handle.mode not in ('r', 'rb'):
#          # otherwise: "OSError: [Errno 9] Bad file descriptor"!
#          os.fsync(file_handle.fileno())
#      file_handle.close()
#
#  #--------------------------------------------
#
#  def cleanup( f_names_list ):
#      global os
#      for i in range(len( f_names_list )):
#         if type(f_names_list[i]) == file:
#            del_name = f_names_list[i].name
#         elif type(f_names_list[i]) == str:
#            del_name = f_names_list[i]
#         else:
#            print('Unrecognized type of element:', f_names_list[i], ' - can not be treated as file.')
#            del_name = None
#
#         if del_name and os.path.isfile( del_name ):
#             os.remove( del_name )
#
#  #--------------------------------------------
#
#  if fb_arch=='Classic':
#      #  {20,20}=72"; {32,10}=52" - but can hang!; {30,8)=46"; {30,15}=93"; {33,10}=91"; {35,5}=no-return-from-python!
#      planned_attachments=30
#      reconnect_count=8
#  elif fb_arch=='SuperClassic':
#      # {20,5}=21"; {30,20}=45"; {50,20}=70"
#      planned_attachments=40
#      reconnect_count=20
#  else:
#      # SS: {30,10}=35"; {50,20}=54"; {40,30}=57"; {75,30}=70"; {80,10}=42"
#      planned_attachments=80
#      reconnect_count=10
#
#
#  subprocess.check_output([ context['fbsvcmgr_path'], "localhost:service_mgr",
#                    "action_properties",
#                    "prp_write_mode", "prp_wm_async",
#                    "dbname", dbfile ], stderr=subprocess.STDOUT)
#
#  # GENERATE CODE FOR EXECUTING IN SEPARATE EXECUTABLE PYTHON CONTEXT
#  ###################################################################
#
#  f_parallel_txt='''import os
#  import fdb
#  import time
#  import datetime
#  from datetime import datetime
#  import subprocess
#  from subprocess import Popen
#  from multiprocessing import Process
#
#  def attach_detach(a_dsn, a_fb_client, v_temp_dir, reconnect_count, process_seq):
#      f_detach_info_sql = open( os.path.join(v_temp_dir, 'tmp_5034_after_detach_' + str(process_seq).zfill(3) + '.log'), 'w')
#      v_role = 'R_'+str(process_seq).zfill(3)
#      v_sqllst = []
#
#      for i in range(0,reconnect_count):
#          v_seq = 100000  + (1+process_seq) * 1000 + 2*i
#
#          att = fdb.connect( dsn = a_dsn, fb_library_name = a_fb_client, user='TMP$C5034', password='123',role = v_role )
#
#          # Trigger 'trg_disc' will add row to LOG4DETACH table.
#          # Column RNO will have value = 'R_nnnn' (for worker #0 --> 'R_000', #1 --> 'R_001' etc),
#          # i.e. it will be NOT null for the timestamp when we are DISCONNECTED to database:
#          att.close()
#
#          v_seq = v_seq + 1
#
#          # Catch current timestamp (we just retuirned from DB connect) and store it in the list:
#          v_sqllst.append( "insert into log4detach(dts, rno, seq) values( '%%s', '%%s', %%s ); " %% ( datetime.strftime(datetime.now(), '%%Y-%%m-%%d %%H:%%M:%%S.%%f')[:23], v_role, v_seq ) )
#
#      # Current worker COMPLETED <reconnect_count> iterations of connect/disconnect,
#      # now we can save timestamps that was stores in the list just after each detach
#      # to the text file for further executing it by ISQL:
#      f_detach_info_sql.write("\\\\n".join(v_sqllst))
#      f_detach_info_sql.write( '\\\\n' )
#
#      merge_sql="merge into log4detach t        using (            select id, %%s + 2 * (row_number()over(order by id)-1) seq            from log4detach d             where rno='%%s'                  and seq is null        ) s        on (s.id = t.id)        when matched then            update set t.seq = s.seq;" %% ( 100000 + (1+process_seq) * 1000, v_role )
#
#      f_detach_info_sql.write( ' '.join(merge_sql.split()).lstrip() )
#      f_detach_info_sql.write( '\\\\n' )
#      f_detach_info_sql.write( 'commit;' )
#      f_detach_info_sql.close()
#
#  planned_attachments = %(planned_attachments)s
#
#  if __name__ == '__main__':
#      p_list=[]
#      v_fb_home = r'%(FB_HOME)s'
#      v_temp_dir = r'%(FBT_TEMP_DIR)s'
#      v_dsn = r'%(dsn)s'
#      v_fb_client = r'%(FB_CLNT)s'
#
#      for i in range(0, planned_attachments):
#          # Python multi-processing feature:
#          ##################################
#          p_i = Process(target=attach_detach, args=( v_dsn, v_fb_client, v_temp_dir, %(reconnect_count)s, i, ))
#          p_i.start()
#          p_list.append(p_i)
#
#      for i in range(len(p_list)):
#          p_list[i].join()
#
#      # All completed
#
#      f_detach_info_sql = open( os.path.join(v_temp_dir, 'tmp_5034_after_detach_all.sql'), 'w')
#      for i in range(len(p_list)):
#          f_detach_log_i = os.path.join(v_temp_dir, 'tmp_5034_after_detach_' + str(i).zfill(3) + '.log')
#          with open(f_detach_log_i, 'r') as s:
#              f_detach_info_sql.write(s.read()+'\\\\n\\\\n\\\\n')
#          os.remove(f_detach_log_i)
#
#      f_detach_info_sql.flush()
#      os.fsync(f_detach_info_sql.fileno())
#      f_detach_info_sql.close()
#
#      # subprocess.call( [ os.path.join( v_fb_home, 'isql'), v_dsn, '-user', '%(user_name)s', '-password', '%(user_password)s', '-nod', '-n', '-i', f_detach_info_sql.name] )
#      subprocess.call( [ r'%(ISQL_BINARY)s', v_dsn, '-user', '%(user_name)s', '-password', '%(user_password)s', '-nod', '-n', '-i', f_detach_info_sql.name] )
#
#      os.remove(f_detach_info_sql.name)
#  ''' % dict(globals(), **locals())
#  # (context['temp_directory'].replace('\\\\','\\\\\\\\'), planned_attachments, dsn, reconnect_count, dsn)
#
#  f_parallel_py=open( os.path.join(context['temp_directory'],'tmp_5034_after_detach.py'), 'w')
#  f_parallel_py.write(f_parallel_txt)
#  flush_and_close( f_parallel_py )
#
#  ########################################################################################################
#  ###    l a u n c h     P y t h o n    i n    a n o t h e r    e x e c u t i o n     c o n t e x t    ###
#  ########################################################################################################
#  runProgram( sys.executable, [f_parallel_py.name] )
#
#  time.sleep(2)
#
#  f_top_slow_sql=open( os.path.join(context['temp_directory'], 'tmp_5034_slowest_detaches.sql'), 'w')
#  f_top_slow_sql.write('drop user tmp$c5034; commit; set count on; set heading off; select * from v_top10_slow; commit;')
#  flush_and_close( f_top_slow_sql )
#
#  f_top_slow_log=open( os.path.join(context['temp_directory'], 'tmp_5034_slowest_detaches.log'), 'w')
#  subprocess.call( [ context['isql_path'], dsn, "-nod", "-n", "-i", f_top_slow_sql.name], stdout=f_top_slow_log, stderr=subprocess.STDOUT)
#  flush_and_close( f_top_slow_log )
#
#  time.sleep(1)
#
#  with open(f_top_slow_log.name) as f:
#      print(f.read())
#
#  # Cleanup.
#  #########
#  time.sleep(1)
#  cleanup( (f_top_slow_sql,f_top_slow_log,f_parallel_py,f_parallel_py.name+'c') )
#
#
#---
