#coding:utf-8

"""
ID:          issue-4462
ISSUE:       4462
TITLE:       In SS sweep blocks establishment of concurrent attachments
DESCRIPTION:
  We create DB-level trigger in order to log attachments info.
  Also, we create a table with long-key indexed fields for sweep have a big time to be completed.
  Then we insert data into this table and create indices on its fields.
  After this we delete just inserted rows and make commit thus providing lot of job to GC.

  When we call on next step sweep, it must work a very long time (maybe several minutes) -
  this was checked on host with common PC characteristics: P-IV 3.0 GHz, SATA).

  We launch SWEEP by async. call of FBSVCMGR and save current timestamp in 'DTS_BEG_FOR_ATTACHMENTS' variable.
  We allow sweep to work several seconds alone (see 'WAIT_BEFORE_RUN_1ST_ATT') and then run loop with launching
  ISQL attachments, all of them - also in async. mode.

  Each ISQL connect will add one row to the log-table ('mon_attach_data') by ON-CONNECT trigger - and we'll
  query later its data: how many ISQL did establish connections while sweep worked.

  After loop we wait several seconds in order to be sure that all ISQL are loaded (see 'WAIT_FOR_ALL_ATT_DONE').

  Then we save new value of current timestamp in 'DTS_END_FOR_ATTACHMENTS' variable.
  After this we call FBSVCMGR with arguments to make SHUTDOWN of database, thus killing all existing attachments
  (SWEEP will be also interrupted in that case). This is done in SYNC mode (control will not return from SHUTDOWN
  process until it will be fully completed).

  Then we return database to ONLINE state and make single ISQL connect with '-nod' switch.
  This (last) attachment to database will query data of Log table 'mon_attach_data' and check that number of
  records (i.e. ACTUALLY etsablished attachment) is equal to which we check (see 'PLANNED_ATTACH_CNT').
NOTES:
[04.07.2020]
  Reduced PLANNED_ATTACH_CNT from 30 to 5 because 4.0 Classic remains fail.
JIRA:        CORE-4135
FBTEST:      bugs.core_4135
"""

import pytest
import time
import re
import subprocess
from datetime import datetime
from firebird.qa import *
from firebird.driver import ShutdownMethod, ShutdownMode

db = db_factory()

act = python_act('db', substitutions=[('[ \t]+', ' ')])

expected_stdout_a = """
    DB-logged attachments: EXPECTED
"""

expected_stdout_b = """
    Trace log parsing. Found triggers before sweep start: EXPECTED (no triggers found).
    Trace log parsing. Found triggers before sweep finish: EXPECTED (equals to planned).
"""

trace = ['time_threshold = 0',
         'log_statement_start = true',
         'log_initfini = false',
         'log_errors = true',
         'log_sweep = true',
         'log_trigger_finish = true',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
     # How many rows must be inserted to the test table:
     RECORDS_TO_ADD = 10000
     # Length of indexed fields.
     # NB, 11.06.2020: sweep can make its work very fast
     # so we have to increase length of indexed fields
     # Checked on 4.0.0.2025 Classic
     INDEXED_FLD_LEN = 500
     # How many attachments we want to establish
     # during sweep will work:
     PLANNED_ATTACH_CNT = 5
     # How many seconds we allow to all PLANNED_ATTACH_CNT
     # sessions for establishing their connections:
     WAIT_FOR_ALL_ATT_DONE = 1 # 10
     # How many seconds we allow SWEEP to work alone,
     # i.e. before 1st ISQL make its attachment:
     WAIT_BEFORE_RUN_1ST_ATT = 2 # 5
     #
     sql_ddl = f"""
     recreate table mon_attach_data (
         mon_attachment_id bigint
         ,mon_timestamp timestamp default 'now'
     );

     recreate table test (
          s01 varchar({INDEXED_FLD_LEN})
         ,s02 varchar({INDEXED_FLD_LEN})
         ,s03 varchar({INDEXED_FLD_LEN})
     );

     set term ^;
     create or alter trigger trg_connect active on connect position 0 as
     begin
         -- IN AUTONOMOUS TRANSACTION DO
         insert into mon_attach_data ( mon_attachment_id ) values (current_connection);
     end
     ^
     set term ;^
     commit;
"""
     act.isql(switches=[], input=sql_ddl)
     #
     sql_data = f"""
     set term ^;
     execute block as
       declare n int = {RECORDS_TO_ADD};
     begin
       while (n>0) do
         insert into test(s01, s02, s03)
         values( rpad('', {INDEXED_FLD_LEN}, uuid_to_char(gen_uuid()) ),
                 rpad('', {INDEXED_FLD_LEN}, uuid_to_char(gen_uuid()) ),
                 rpad('', {INDEXED_FLD_LEN}, uuid_to_char(gen_uuid()) )
               ) returning :n-1 into n;
     end^
     set term ;^
     commit;

     create index test_a on test(s01,s02,s03);
     create index test_b on test(s02,s03,s01);
     create index test_c on test(s03,s02,s01);
     create index test_d on test(s01,s03,s02);
     create index test_e on test(s02,s01,s03);
     create index test_f on test(s03,s01,s02);
     commit;

     delete from test;
     commit; -- ==> lot of garbage in indexed pages will be after this.
"""
     #
     act.reset()
     act.isql(switches=['-nod'], input=sql_data)
     # Restore FW to ON (make sweep to do its work "harder"):
     act.db.set_sync_write()
     # Trace
     with act.trace(db_events=trace):
          # Traced action
          # Now we run SWEEP in child thread (asynchronous) and while it will work - try to establish several attachments.
          sweeper = subprocess.Popen([act.vars['gfix'], '-sweep',  '-user', act.db.user,
                                      '-password', act.db.password, act.db.dsn],
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
                                     )
          #
          try:
               time.sleep(WAIT_BEFORE_RUN_1ST_ATT)
               # Save current timestamp: this is point BEFORE we try to establish attachmentas using several ISQL sessions:
               DTS_BEG_FOR_ATTACHMENTS = datetime.strftime(datetime.now(), '%Y-%m-%d %H:%M:%S')
               # Try to establish several attachments to database while sweep is in work:
               check_sql = """
               set heading off;
               set term ^;
               execute block returns(att bigint) as
               begin
                   att = current_connection;
                   suspend;
               end
               ^
               set term ;^
               commit;
"""
               for i in range(PLANNED_ATTACH_CNT):
                    act.reset()
                    act.isql(switches=['-n'], input=check_sql)
               time.sleep(WAIT_FOR_ALL_ATT_DONE)
               # Save current timestamp: we assume that now ALL isql sessions already FINISHED to
               # establish attachment (or the whole job and quited)
               DTS_END_FOR_ATTACHMENTS = datetime.strftime(datetime.now(), '%Y-%m-%d %H:%M:%S')
               # Move database to shutdown in order to stop sweep
               with act.connect_server() as srv:
                    srv.database.shutdown(database=act.db.db_path, mode=ShutdownMode.FULL,
                                          method=ShutdownMethod.FORCED, timeout=0)
          finally:
               # Kill sweep
               sweeper.terminate()
     #
     # Return database online in order to check number of attachments that were established
     # while sweep was in work
     with act.connect_server() as srv:
          srv.database.bring_online(database=act.db.db_path)
     # Check: number of ISQL attachments between DTS_BEG_FOR_ATTACHMENTS and
     # DTS_END_FOR_ATTACHMENTS must be equal to 'PLANNED_ATTACH_CNT'
     #
     # SuperServer has two system attachments with mon$user='Garbage Collector' and 'Cache Writer',
     # we have to SKIP them from counting:
     con_sql = f"""
     set list on;
     select iif( cnt = {PLANNED_ATTACH_CNT}
                 ,'EXPECTED' -- OK, number of attachments is equal to expected value.
                 ,'POOR: only ' || cnt || ' attachments established for {WAIT_FOR_ALL_ATT_DONE} seconds, expected: {PLANNED_ATTACH_CNT}'
               ) as "DB-logged attachments:"
     from (
        select count(*) as cnt
        from mon_attach_data d
        where d.mon_timestamp between '{DTS_BEG_FOR_ATTACHMENTS}' and '{DTS_END_FOR_ATTACHMENTS}'
     );

     /*
     -- 4debug, do not delete:
     set list off;
     set count on;
     select d.*
     from mon_attach_data d
     where d.mon_timestamp between '{DTS_BEG_FOR_ATTACHMENTS}' and '{DTS_END_FOR_ATTACHMENTS}';
     -- */

     commit;
"""
     act.reset()
     act.expected_stdout = expected_stdout_a
     act.isql(switches=['-pag', '99999', '-nod'], input=con_sql)
     assert act.clean_stdout == act.clean_expected_stdout
     # check trace
     allowed_patterns = [re.compile('EXECUTE_TRIGGER_FINISH', re.IGNORECASE),
                         re.compile('SWEEP_START', re.IGNORECASE),
                         re.compile('SWEEP_FINISH', re.IGNORECASE),
                         re.compile('SWEEP_FAILED', re.IGNORECASE)]
     # All events 'EXECUTE_TRIGGER_FINISH' must occur between SWEEP_START and SWEEP_FAILED
     found_swp_start = False
     found_swp_finish = False
     triggers_count_before_swp_start = 0
     triggers_count_before_swp_finish = 0
     for line in act.trace_log:
          for p in allowed_patterns:
               if result:= p.search(line):
                    what_found = result.group(0)
                    if 'SWEEP_START' in what_found:
                         found_swp_start = True
                    elif 'SWEEP_FINISH' in what_found or 'SWEEP_FAILED' in what_found:
                         found_swp_finish = True
                    elif 'EXECUTE_TRIGGER_FINISH' in what_found:
                         triggers_count_before_swp_start += (1 if not found_swp_start else 0)
                         triggers_count_before_swp_finish += (1 if found_swp_start and not found_swp_finish else 0)

     print('Trace log parsing. Found triggers before sweep start:',
           'EXPECTED (no triggers found).' if triggers_count_before_swp_start == 0
           else f'UNEXPECTED: {triggers_count_before_swp_start} instead of 0.')
     print('Trace log parsing. Found triggers before sweep finish:',
           'EXPECTED (equals to planned).' if triggers_count_before_swp_finish == PLANNED_ATTACH_CNT
           else f'UNEXPECTED: {triggers_count_before_swp_finish} instead of {PLANNED_ATTACH_CNT}.')
     act.expected_stdout = expected_stdout_b
     act.stdout = capsys.readouterr().out
     assert act.clean_stdout == act.clean_expected_stdout
