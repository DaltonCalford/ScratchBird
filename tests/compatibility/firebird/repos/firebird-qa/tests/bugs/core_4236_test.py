#coding:utf-8

"""
ID:          issue-4560
ISSUE:       4560
TITLE:       Database shutdown is reported as successfully completed before all active connections are in fact interrupted
DESCRIPTION:
  Test restores database with single table of following DDL:
    create table test(s varchar(1000));
    create index test_s on test(s);
  Then we start asynchronously several ISQL attachments which will do 'heavy DML' job: insert lot of rows in this table.
  After some delay (IMO, it should be at least 15..20 seconds) we start process of SHUTDOWN but with target mode = 'single'
  instead of 'full'.
  After control will return from shutdown command, we can ensure that database has no any access and its file is closed
  - this is done by call FBSVCMGR utility with arguments: action_repair rpr_validate_db rpr_full. This will launch process
  of database validation and it requires exclusive access, like 'gfix -v -full'.
  If validation will be able to open database in exclusive mode and do its job then NO any output must appear.

  Any problem with exclusive DB access will lead to error with text like: "database <db_file> shutdown".
  Finally, we check that:
  1) output of validation is really EMPTY - no any rows should present between two intentionally added lines
    ("Validation start" and "validation finish" - they will be added only to improve visual perception of log);
  2) Every launched ISQL was really able to perform its task: at least to insert 100 row in the table, this result should
     be reflected in its log by message 'INS_PROGRESS ...' - it is suspended from EB every 100 rows.
  3) Every launched ISQL was really received exception with SQLCODE = HY000 - it also should be added at the end of ISQL log.

  Checked on WI-V3.0.0.32253, SS/SC/CS (OS=Win XP), with 30 attachments that worked for 30 seconds.
  NB: Issue about hang CS was found during this test implementation, fixed here:
  http://sourceforge.net/p/firebird/code/62737

  Refactored 13-aug-2020:
    validation result is verified by inspecting difflib.unified_diff() result between firebird.log that was
    before and after validation: it must contain phrase "Validation finished: 0 errors"
    (we check that both validation *did* complete and absense of errors in DB).
JIRA:        CORE-4236
FBTEST:      bugs.core_4236
NOTES:
    [13.09.2022] pzotov
    Validation outcome may contain info about WARNINGS (found in 3.0.8 SS and 4.0.1 SS), like this:
    "Number of index page warnings   : NN"
    This outcome is considered as EXCEPTION, so we have to prevent from fail because of that.
    We must raise fail only in case of message like this: "Number of <index | record level | ...> errors   : NN"
    Checked on Windows: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.691
"""

from __future__ import annotations
from typing import List
import pytest
from subprocess import run, CompletedProcess, STDOUT, PIPE
import re
import time
from threading import Thread, Barrier, Lock
from difflib import unified_diff
from firebird.qa import *
from firebird.driver import ShutdownMode, ShutdownMethod, SrvRepairFlag

db = db_factory(from_backup='core4236.fbk')

act = python_act('db', substitutions=[('VALIDATION FINISHED: 0 ERRORS.*', 'VALIDATION FINISHED: 0 ERRORS')])

expected_stdout = """
    DIFF IN FIREBIRD.LOG: + VALIDATION FINISHED: 0 ERRORS, 0 WARNINGS, 0 FIXED
    Check-1: how many DML attachments really could do their job ?
    Result: OK, launched = actual
    Check-2: how many sessions got SQLSTATE = HY000 on shutdown ?
    Result: OK, launched = actual
"""


def isql_job(act: Action, b: Barrier, lock: Lock, result_list: List[str]):
     dml_script = """
     set bail on;
     set list on;
     set term ^;
     execute block returns(ins_progress int) as
       declare n int = 100000;
     begin
       ins_progress=0;
       while (n > 0) do
       begin
         insert into test(s) values(rpad('', 500, uuid_to_char(gen_uuid())));

         ins_progress = ins_progress + 1;
         if (mod(ins_progress, 100) = 0) then suspend;

         n = n - 1;
       end
     end
     ^ set term ;^
     quit;
     """
     b.wait()
     result: CompletedProcess = run([act.vars['isql'], '-user', act.db.user,
                                     '-password', act.db.password, act.db.dsn],
                                    input=dml_script, encoding='utf8',
                                    stdout=PIPE, stderr=STDOUT)
     with lock:
          result_list.append(result.stdout)


@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):
     PLANNED_DML_ATTACHMENTS = 20
     WAIT_FOR_ALL_CONNECTIONS_START_JOB = 10
     lock = Lock()
     threads = []
     result_list = []
     # Start multiple ISQL instances with heavy DML job
     b = Barrier(PLANNED_DML_ATTACHMENTS + 1)
     for i in range(PLANNED_DML_ATTACHMENTS):
          isql_thread = Thread(target=isql_job, args=[act, b, lock, result_list])
          threads.append(isql_thread)
          isql_thread.start()
     b.wait()
     time.sleep(WAIT_FOR_ALL_CONNECTIONS_START_JOB)
     with act.connect_server() as srv:
          # Move database to shutdown with ability to run after it validation (prp_sm_single)
          srv.database.shutdown(database=act.db.db_path, mode=ShutdownMode.SINGLE,
                                method=ShutdownMethod.FORCED, timeout=0)
          # get firebird.log _before_ validation
          srv.info.get_log()
          log_before = srv.readlines()
          # At this point no further I/O should be inside database, including internal engine actions
          # that relate to backouts. This mean that we *must* have ability to run DB validation in
          # _exclusive_ mode, like gfix -v -full does.
          #
          # Run validation that requires exclusive database access.
          # This process normally should produce NO output at all, it is "silent".
          # If database currently is in use by engine or some attachments than it shoudl fail
          # with message "database <db_file> shutdown."

          try:
              srv.database.repair(database=act.db.db_path,
                                   flags=SrvRepairFlag.FULL | SrvRepairFlag.VALIDATE_DB)
          except Exception as exc:
              # Prevent from 'fail-alarm' related to WARNINGS found during validation.
              # We are interesting only about real errors:
              p_validation_problem_msg = re.compile('number of .* errors', re.IGNORECASE)
              if p_validation_problem_msg.search(exc.__str__().lower()):
                  print(f'Database validation failed: {exc}')

          # get firebird.log _after_ validation
          srv.info.get_log()
          log_after = srv.readlines()

          # bring database online
          srv.database.bring_online(database=act.db.db_path)
     # At this point, threads should be dead
     for thread in threads:
          thread.join(1)
          if thread.is_alive():
               print(f'Thread {thread.ident} is still alive')
     # Compare logs
     log_diff = list(unified_diff(log_before, log_after))
     # We are ionterested only for lines that contains result of validation:
     p = re.compile('validation[	 ]+finished(:){0,1}[	 ]+\\d+[	 ]errors', re.IGNORECASE)
     for line in log_diff:
          if line.startswith('+') and p.search(line):
               print('DIFF IN FIREBIRD.LOG: ' + (' '.join(line.split()).upper()))
     #
     actual_dml_attachments = 0
     logged_shutdown_count = 0
     for sqllog in result_list:
          if 'INS_PROGRESS' in sqllog:
               actual_dml_attachments += 1
          if 'SQLSTATE = HY000' in sqllog:
               logged_shutdown_count += 1
     #
     print("Check-1: how many DML attachments really could do their job ?")
     if PLANNED_DML_ATTACHMENTS == actual_dml_attachments:
          print("Result: OK, launched = actual")
     else:
          print("Result: BAD, launched<>actual")
     #
     print("Check-2: how many sessions got SQLSTATE = HY000 on shutdown ?")
     if PLANNED_DML_ATTACHMENTS == logged_shutdown_count:
          print("Result: OK, launched = actual")
     else:
          print("Result: BAD, launched<>actual")
     # Check
     act.expected_stdout = expected_stdout
     act.stdout = capsys.readouterr().out
     assert act.clean_stdout == act.clean_expected_stdout
