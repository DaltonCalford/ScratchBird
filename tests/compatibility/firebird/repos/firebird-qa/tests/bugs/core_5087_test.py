#coding:utf-8

"""
ID:          issue-5372
ISSUE:       5372
TITLE:       Database shutdown can cause server crash if multiple attachments run EXECUTE STATEMENT
DESCRIPTION:
     Test makes two copies of current DB file (and closes db_conn after this):
     1) to <db_dml_sessions> - for use by launching ISQL sessions which will perform "heavy DML" (insert rows in the table with wide-key index);
     2) to <db_verify_alive> - for use to verify that FB is alive after we change state of <db_dml_sessions> while ISQL sessions running.

     Then we establish connect to <db_verify_alive> database (see 'att_chk').
     If FB is alive after DML and shutdown then we have to see alive connection to <db_verify_alive>.
     If FB works as SuperServer/SuperClassic and crashed then this connect will be lost and test fails with Python exception.
     If FB works as Classic then this connect will alive but we can check firebird.log for presense of phrases which testify about crash:
     "code attempted to access" and/or "terminate(d) abnormally". We analyze the DIFFERENCE file between old and new firebird.log for this.

     After this, we establish connection to <db_dml_sessions> (see 'att1') and make INSERT statement to special table 't_lock' which has PK-field.
     Futher, we launch <PLANNED_DML_ATTACHMENTS> ISQL sessions which also must insert the same (duplicated) key into this table - and they hang.
     Table 't_lock' serves as "semaphore": when we close attachment 'att1' then all running ISQL sessions will immediately do their DML job.

     We check that all required number connections of ISQLs present by loop in which we query mon$attachments table and count records from it.
     When result of this count will be equal to <PLANNED_DML_ATTACHMENTS> then we release lock from T_LOCK table by closing 'att1'.

     Futher, we allow ISQL sessions to perform their job by waiting for several seconds and then run command to change DB state to full shutdown.
     This command works without returning to caller until all activity in the DB will stop.

     When control will return here, we check that attachment 'att_chk' is alive. Also, we get difference of FB log content and parse by searching
     phrases which can relate to FB crash.

     We also do additional check: all ISQL sessions should be disconnected with writing to logs appropriate messages about shutdown.
JIRA:        CORE-5087
FBTEST:      bugs.core_5087
NOTES:
    [14.09.2022] pzotov
    NB: ticket was created at 28-jan-2016 after testing on LI-V3.0.0.32294 (Release Candidate 1).
    QA suite can not run with such  too old snapshot and fails with:
        firebird.driver.types.DatabaseError: invalid format for transaction parameter block
        wrong version of transaction parameter block
    Checked on Linux and Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.591
"""

from __future__ import annotations
from typing import List
import pytest
import subprocess
import time
import re
from difflib import unified_diff
from pathlib import Path
from firebird.qa import *
from firebird.driver import ShutdownMode, ShutdownMethod

init_script = """
    create sequence g;
    recreate table test(
        id int,
        s varchar(500) unique,
        att bigint default current_connection
    );
    recreate table log4attach(
        att bigint default current_connection
        ,dts timestamp default 'now'
        ,process varchar(255)
        ,protocol varchar(255)
        ,address varchar(255)
    );
    commit;

    set term ^;
    execute block as
    begin
        rdb$set_context('USER_SESSION','INITIAL_DDL','1');
    end
    ^

    create or alter trigger trg_attach active on connect position 0 as
    begin
        if ( rdb$get_context('USER_SESSION','INITIAL_DDL') is null )
        then
            in autonomous transaction do
                insert into log4attach(process,protocol, address)
                values(  rdb$get_context('SYSTEM', 'CLIENT_PROCESS')
                        ,rdb$get_context('SYSTEM', 'NETWORK_PROTOCOL')
                        ,rdb$get_context('SYSTEM', 'CLIENT_ADDRESS')
                      );
    end
    ^ -- trg_attach
    set term ;^
    commit;

    create index test_att on test(att);
    create index test_id on test(id);
    create index log4attach_att on log4attach(att);
    commit;
"""

db_verify_alive = db_factory(sql_dialect=3, init=init_script, filename='tmp_5087_chk4alive.fdb')
db_dml_sessions = db_factory(sql_dialect=3, init=init_script, filename='tmp_5087_dml_heavy.fdb')

act = python_act('db_verify_alive')

expected_stdout = """
    check_point_1: established connection to <db_verify_alive>
    Found all attachments of planned number to <db_dml_sessions>. Now we can release lock for allow them to start DML.
    check_point_2: before shutdown <db_dml_sessions>
    check_point_3: after shutdown <db_dml_sessions>
    check_point_4: killed all DML sessions.
    check_point_5: get firebird.log
    distinct attachments to <db_verify_alive>: 1
    Found crash messages in DML worker logs: 0
    Point before bring DML database online.
    Point after bring DML database online.
    All DML sessions found in DB ?  YES
"""

PLANNED_DML_ATTACHMENTS = 30
TIME_FOR_DML_WORK = 12 # Seconds to sleep. Was 4, but 12 required on my system for all DML to show up

dml_logs = temp_files([f'tmp_dml_5087_{i+1}' for i in range(PLANNED_DML_ATTACHMENTS)])
check_log = temp_file('tmp_chk_5087.log')
dml_script = temp_file('dml-script.sql')

chk_script = f"""
     set list on;
     select
        iif(  count(distinct t.att) = {PLANNED_DML_ATTACHMENTS}
             ,'YES'
             ,'NO. Only ' || count(distinct t.att) || ' attachments of planned {PLANNED_DML_ATTACHMENTS} established.')
        as "All DML sessions found in DB ?"
     from rdb$database r
     left join (
         select
             att
             ,count(iif(id >=0, id, null)) as cnt_in_autonom_tx
             ,count(iif(id < 0, id, null)) as cnt_in_common_tx
         from test
         group by att
     ) t on t.cnt_in_common_tx = 0 and t.cnt_in_autonom_tx > 0;
     """

@pytest.mark.version('>=3.0')
def test_1(act: Action, db_dml_sessions: Database, dml_logs: List[Path],
           check_log: Path, dml_script: Path, capsys):
     #
     dml_script.write_text("""
     commit;
     set transaction wait;
     set term ^;
     execute block as
     begin
         insert into t_lock(id) values(1);
         when any do
         begin
          -- nop --
         end
     end
     ^
     set term ;^
     commit;

     set bail on;

     set transaction read committed;
     set term ^;
     execute block as
         declare n_limit int = 100000;
         declare s_length smallint;
     begin
         select ff.rdb$field_length
         from rdb$fields ff
         join rdb$relation_fields rf on ff.rdb$field_name = rf.rdb$field_source
         where rf.rdb$relation_name=upper('test') and rf.rdb$field_name=upper('s')
         into s_length;

         while (n_limit > 0) do
         begin
             execute statement ('insert into test(id, s) values( ?, ?)')
                   ( gen_id(g,1), rpad('', :s_length, uuid_to_char(gen_uuid()))  )
                   with autonomous transaction
             ;

             n_limit = n_limit - 1;
         end
         insert into test( id ) values( -current_connection );
     end
     ^
     set term ;^
     commit;
     """)
     #
     with open(check_log, mode='w') as f_check_log:
          with act.db.connect() as att_chk:
               att_chk.execute_immediate('delete from log4attach where att<>current_connection')
               att_chk.commit()
               att_chk.begin()
               cur_chk = att_chk.cursor()
               cur_chk.execute("select 'check_point_1: established connection to <db_verify_alive>' as msg from rdb$database")
               for row in cur_chk:
                    f_check_log.write(f'{row[0]}\n')
               att_chk.commit()
               #
               f_logs = []
               p_dml = []
               try:
                    with db_dml_sessions.connect() as att1, db_dml_sessions.connect() as att2:
                         att1.execute_immediate('recreate table t_lock(id int primary key)')
                         att1.commit()
                         att1.execute_immediate('insert into t_lock(id) values(1)')
                         with act.connect_server() as srv:
                              srv.info.get_log()
                              log_before = srv.readlines()
                         # Launch isql processes for DML
                         for dml_log in dml_logs: # Contains PLANNED_DML_ATTACHMENTS items
                              f = open(dml_log, mode='w')
                              f_logs.append(f)
                              p_dml.append(subprocess.Popen([act.vars['isql'],
                                                             '-i', str(dml_script),
                                                             '-user', act.db.user,
                                                             '-password', act.db.password,
                                                             db_dml_sessions.dsn],
                                                            stdout=f, stderr=subprocess.STDOUT))
                         #
                         cur2 = att2.cursor()
                         while True:
                              att2.begin()
                              cur2.execute("select count(*) from mon$attachments a where a.mon$attachment_id<>current_connection and a.mon$remote_process containing 'isql'")
                              established_dml_attachments = cur2.fetchone()[0]
                              att2.commit()
                              #
                              if established_dml_attachments < PLANNED_DML_ATTACHMENTS:
                                   # do not delete, leave it for debug:
                                   ####################################
                                   #msg = datetime.datetime.now().strftime("%H:%M:%S.%f") + ' LOOP: only %(established_dml_attachments)s attachments exist of planned %(PLANNED_DML_ATTACHMENTS)s' % locals()
                                   #print( msg )
                                   #f_check_log.write(f'{msg}\n')
                                   time.sleep(1)
                                   continue
                              else:
                                   # do not delete, leave it for debug:
                                   ####################################
                                   #msg = datetime.datetime.now().strftime("%H:%M:%S.%f") + ' Found all planned %(PLANNED_DML_ATTACHMENTS)s attachments. Can release lock.' % locals()
                                   msg = 'Found all attachments of planned number to <db_dml_sessions>. Now we can release lock for allow them to start DML.'
                                   #print( msg )
                                   f_check_log.write(f'{msg}\n')
                                   break
                    # All launched ISQL sessions can do useful job (INSERT statements) since this point.
                    # Let ISQL sessions do some work:
                    time.sleep(TIME_FOR_DML_WORK)
                    cur_chk.execute("select 'check_point_2: before shutdown <db_dml_sessions>' as msg from rdb$database")
                    for row in cur_chk:
                         f_check_log.write(f'{row[0]}\n')
                    att_chk.commit()
                    # Shutdown database
                    with act.connect_server() as srv:
                         srv.database.shutdown(database=db_dml_sessions.db_path,
                                               mode=ShutdownMode.FULL, method=ShutdownMethod.FORCED,
                                               timeout=0)
                    cur_chk.execute("select 'check_point_3: after shutdown <db_dml_sessions>' as msg from rdb$database")
                    for row in cur_chk:
                         f_check_log.write(f'{row[0]}\n')
                    att_chk.commit()
               finally:
                    for f in f_logs:
                         f.close()
                    for p in p_dml:
                         p.terminate()
               #
               cur_chk.execute("select 'check_point_4: killed all DML sessions.' as msg from rdb$database")
               for row in cur_chk:
                    f_check_log.write(f'{row[0]}\n')
               att_chk.commit()
               #
               # Here we must wait a little because firebird.log will get new messages NOT instantly.
               time.sleep(3)
               #
               crashes_in_worker_logs = 0
               for dml_log in dml_logs:
                    if 'SQLSTATE = 08004' in dml_log.read_text():
                         crashes_in_worker_logs += 1
               #
               with act.connect_server() as srv:
                    srv.info.get_log()
                    log_after = srv.readlines()
               att_chk.begin()
               cur_chk.execute("select 'check_point_5: get firebird.log' as msg from rdb$database")
               for row in cur_chk:
                    f_check_log.write(f'{row[0]}\n')
               att_chk.commit()
               #
               cur_chk.execute('select count(distinct att) from log4attach;')
               for row in cur_chk:
                    f_check_log.write(f'distinct attachments to <db_verify_alive>: {row[0]}\n') # must be 1
          #
          f_check_log.write(f'Found crash messages in DML worker logs: {crashes_in_worker_logs}\n') # must be 0.
          f_check_log.write('Point before bring DML database online.\n')
          with act.connect_server() as srv:
               srv.database.bring_online(database=db_dml_sessions.db_path)
          f_check_log.write('Point after bring DML database online.\n')
          act.isql(switches=['-nod', str(db_dml_sessions.dsn)], input=chk_script, connect_db=False)
          f_check_log.write(act.stdout)
          # Now we can compare two versions of firebird.log and check their difference.
          allowed_patterns = [re.compile('code attempt\\S+.*\\s+access',re.IGNORECASE),
                              re.compile('terminate\\S+ abnormally',re.IGNORECASE),
                              re.compile('Error writing data',re.IGNORECASE)
                              ]
          for line in unified_diff(log_before, log_after):
              # ::: NB :::
              # filter(None, [p.search(line) for p in allowed_patterns]) will be None only in Python 2.7.x!
              # In Python 3.x this will retrun "<filter object at 0xNNNNN>" ==> we must NOT use this statement!
              if line.startswith('+') and act.match_any(line, allowed_patterns):
                  f_check_log.write(f'Crash message in firebird.log detected: {line.upper()}\n')
                  # !! DOES NOT WORK IN Python 3.x !!
                  # if filter(None, [p.search(line) for p in allowed_patterns]):
                  #     f_check_log.write(f'Crash message in firebird.log detected: {line.upper()}\n')

     # Final check
     act.expected_stdout = expected_stdout
     act.stdout = check_log.read_text()
     assert act.clean_stdout == act.clean_expected_stdout
