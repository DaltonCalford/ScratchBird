#coding:utf-8

"""
ID:          transactions.read-consist-sttm-restart-on-merge-03
TITLE:       READ CONSISTENCY. Check creation of new statement-level snapshot and restarting changed caused by MERGE. Test-03.
DESCRIPTION:
  Initial article for reading:
          https://asktom.oracle.com/pls/asktom/f?p=100:11:::::P11_QUESTION_ID:11504247549852
          Note on terms which are used there: "BLOCKER", "LONG" and "FIRSTLAST" - their names are slightly changed here
          to: LOCKER-1, WORKER and LOCKER-2 respectively.
      See also: doc/README.read_consistency.md

      **********************************************

      This test verifies that statement-level snapshot and restart will be performed when "main" session ("worker")
      performs DELETE statement and is involved in update conflicts.
      ("When update conflict is detected <...> then engine <...> creates new statement-level snapshot and restart execution...")

      ::: NB :::
      This test uses script %FBT_REPO%/files/read-consist-sttm-restart-DDL.sql which contains common DDL for all other such tests.
      Particularly, it contains two TRIGGERS (TLOG_WANT and TLOG_DONE) which are used for logging of planned actions and actual
      results against table TEST. These triggers use AUTONOMOUS transactions in order to have ability to see results in any
      outcome of test.

      ###############
      Following scenario if executed here (see also: "doc/README.read_consistency.md"; hereafer is marked as "DOC"):

      * add new table that is child to test: TDETL (with FK that references TEST and 'on delete cascade' clause)
      * three rows are inserted into the table TEST, with IDs: 2, 3 and 5.

      * session 'locker-1' ("BLOCKER" in Tom Kyte's article ):
              update set id=id where id = 5;

      * session 'worker' ("LONG" in TK article) has mission:
              merge into test t using(select * from test where id >= 3 order by id) s on t.id = s.id when matched then delete;
              // using TIL = read committed read consistency

          // Execution will have PLAN ORDER <ASCENDING_INDEX>.
          // It will delete (first avaliable for cursor) row with ID = 3 but can not change row with ID = 5 because of locker-1.
          // Update conflict appears here and, because of this, worker temporary changes its TIL to RC no record_version (RC NRV).
          // [DOC]: "a) transaction isolation mode temporarily switched to the READ COMMITTED *NO RECORD VERSION MODE*"
          // This (new) TIL allows worker further to see all committed versions, regardless of its own snapshot.

      * session 'locker-2' ("FIRSTLAST" in TK article): replaces ID = 2 with new value = 4, then commits
        and locks this record again:
              (1) update test set id = 4 where id = 2;
              (2) commit;
              (3) update test set id=id where id = 4;
          // session-'worker' remains waiting at this point because row with ID = 5 is still occupied by by locker-1
          // but worker must further see record with (new) id = 4 because its TIL was changed to RC NO RECORD_VERSION.

      * session 'locker-1':
              (1) commit;
              (2) insert into test(id) values(6);
              (3) insert into detl(id, pid) values(6001, 6);
              (4) commit;
              (5) update test set id=id where id=6;
          // first of these statements: '(1) commit' - will release record with ID = 5.
          // Worker sees this record (because of TIL = RC NRV) and put write-lock on it.
          // [DOC]: "b) engine put write lock on conflicted record"
          // Also, because worker TIL = RC NRV, it will see two new rows with ID = 4 and 6, and they meet worker cursor condition ("id>=3").
          // Worker resumes search for any rows with ID >=3, and it does this with taking in account "ORDER BY ID ASC".
          // [DOC]: "c) engine continue to evaluate remaining records of update/delete cursor and put write locks on it too"
          // Worker starts to search records which must be involved in its DML and *found* sucn rows (with ID = 4 and 6).
          // NB. These rows currently can NOT be deleted by worker because of locker-2 and locker-1 have uncommitted updates.
          // BECAUSE OF FACT THAT AT LEAST ONE ROW *WAS FOUND* - STATEMENT-LEVEL RESTART *NOT* YET OCCURS HERE.
          // :::!! NB, AGAIN !! ::: restart NOT occurs here because at least one records found, see:
          // [DOC]: "d) when there is *no more* records to fetch, engine start to undo all actions performed since
          //            top-level statement execution starts and preserve already taken write locks
          //         e) then engine restores transaction isolation mode as READ COMMITTED *READ CONSISTENCY*,
          //            creates new statement-level snapshot and restart execution of top-level statement."

      * session 'locker-2':
             commit;
          // This will release record with ID = 4 (but row with ID = 6 is still inaccessible because of locker-1).
          // Worker sees record (because of TIL = RC NRV) with ID = 4 and put write-lock on it.
          // Then worker resumes search for any (new) rows with ID >= 3, and it does this with taking in account required order
          // of its DML (i.e. ORDER BY ID ASC).
          // [DOC]: "c) engine continue to evaluate remaining records of update/delete cursor and put write locks on it too"
          // But there are no such rows in the tableL earlier worker already encountered all possible rows (with ID=4 and 6)
          // and *did* put write-locks on them. So at this point NO new rows can be found for putting new lock on it.
          // BECAUSE OF FACT THAT NO RECORDS FOUND, WORKER DOES UNDO BUT KEEP LOCKS AND THEN MAKES FIRST STATEMENT-LEVEL RESTART.
          // [DOC]: "d) when there is no more records to fetch, engine start to undo all actions ... and preserve already taken write locks
          //         e) then engine restores transaction isolation mode as READ COMMITTED *READ CONSISTENCY*,
          //           creates new statement-level snapshot and restart execution of top-level statement."

      * session 'locker-1':
             commit;
          // This will release record with ID = 6 - and this is the last row which meet cursor condition of session-worker.
          // Worker sees record (because of TIL = RC NRV) with ID = 6 and put write-lock on it.
          // Then worker resumes search for any (new) rows with ID >= 3, and it does this with taking in account required order
          // of its DML (i.e. ORDER BY ID ASC). NO new rows (with ID >= 3) can be found for putting new lock on it.
          // BECAUSE OF FACT THAT NO RECORDS FOUND, WORKER DOES UNDO BUT KEEP LOCKS AND THEN MAKES SECOND STATEMENT-LEVEL RESTART.

      Expected result:
      * session-'worker' must *successfully* complete deletion of all rows which it could see at the starting point (ID=3 and 5)
        PLUS rows with ID = 4 (ex. ID=2) and 6  (this ID is new, it did not exist at the statement start).
        As result, all rows must be deleted.

      * three unique values must be in the column TLOG_DONE.SNAP_NO for session-'worker' when it performed DELETE statement: first of them
        was created by initial statement start and all others reflect two restarts (this column has values which are evaluated using
        rdb$get_context('SYSTEM', 'SNAPSHOT_NUMBER') -- see trigger TEST_AIUD).
        It is enough to count these values using COUNT(*) or enumarate them by DENSE_RANK() function.

      NOTE: concrete values of fields TRN, GLOBAL_CN and SNAP_NO in the TLOG_DONE can differ from one to another run!
      This is because of concurrent nature of connections that work against database. We must not assume that these values will be constant.
FBTEST:      functional.transactions.read_consist_sttm_restart_on_merge_03
NOTES:
    [29.07.2022] pzotov
        Checked on 4.0.1.2692, 5.0.0.591
    [25.09.2023] pzotov
        1. Added trace launch and its parsing in order to get number of times when WORKER statement did restart.
           See commits:
           1) FB 4.x (23-JUN-2022, 4.0.2.2782): https://github.com/FirebirdSQL/firebird/commit/95b8623adbf129d0730a50a18b4f1cf9976ac35c
           2) FB 5.x (27-JUN-2022, 5.0.0.555):  https://github.com/FirebirdSQL/firebird/commit/f121cd4a6b40b1639f560c6c38a057c4e68bb3df

           Trace must contain several groups, each with similar lines:
               <timestamp> (<trace_memory_address>) EXECUTE_STATEMENT_RESTART
               {SQL_TO_BE_RESTARTED}
               Restarted <N> time(s)

        2. To prevent raises between concurrent transactions, it is necessary to ensure that code:
               * does not allow LOCKER-2 to start its work until WORKER session will establish connection and - moreover - will actually locks first record 
                 from the scope that is seen by the query that we want to be executed by worker.
               * does not allow LOCKER-1 to do something after LOCKER-2 issued commit (and thus released record): we first have to ensure that this record
                 now is locked by WORKER. The same when record was occupied by LOCKER-2 and then is released: LOCKER-1 must not do smth until WORKER will
                 encounter this record and 'catch' it.
           This is done by calls to function 'wait_for_record_become_locked()' which are performed by separate 'monitoring' connection with starting Tx
           with NO_WAIT mode and catching exception with further parsing. In case when record has been already occupied (by WORKER) this exception will
           have form "deadlock / -update conflicts ... / -concurrent transaction number is <N>". We can then obtain number of this transaction and query
           mon$statements for get MON$SQL_TEXT that is runnig by this Tx. If it contains contains 'special tag' (see variable SQL_TAG_THAT_WE_WAITING_FOR)
           then we can be sure that WORKER really did establish connection and successfully locked row with required ID.

           Table 'TLOG_WANT' (which is fulfilled by trigger TEST_BIUD using in autonomous tx) can NOT be used for detection of moments when WORKER
           actually locks records which he was waiting for: this trigger fires BEFORE actual updating occurs, i.e. when record become seeon by WORKER
           but is still occupied by some LOCKER ("[DOC]: c) engine continue to evaluate remaining records ... and put write locks on it too")

           NB! Worker transaction must running in WAIT mode - in contrary to Tx that we start in our monitoring loop.

        Checked on WI-T6.0.0.48, WI-T5.0.0.1211, WI-V4.0.4.2988.

    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
    Merge expression defined in 'SQL_TO_BE_RESTARTED' must NOT end with semicolon!
"""

import subprocess
import re
from pathlib import Path
import time
import datetime as py_dt
import locale

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, TraAccessMode, DatabaseError

db = db_factory()

act = python_act( 'db', substitutions = [ ('=', ''), ('[ \t]+', ' '), ('.* EXECUTE_STATEMENT_RESTART', 'EXECUTE_STATEMENT_RESTART') ] )
#act = python_act( 'db', substitutions = [ ('.* EXECUTE_STATEMENT_RESTART', 'EXECUTE_STATEMENT_RESTART') ] )

MAX_WAIT_FOR_WORKER_START_MS = 10000;
SQL_TAG_THAT_WE_WAITING_FOR = 'SQL_TAG_THAT_WE_WAITING_FOR'
# SQL_TO_BE_RESTARTED -- will be defined inside loop, see below!

fn_worker_sql = temp_file('tmp_worker.sql')
fn_worker_log = temp_file('tmp_worker.log')
fn_worker_err = temp_file('tmp_worker.err')

#-----------------------------------------------------------------------------------------------------------------------------------------------------

def wait_for_record_become_locked(tx_monitoring, cur_monitoring, sql_to_lock_record, SQL_TAG_THAT_WE_WAITING_FOR):

    # ::: NB :::
    # tx_monitoring must work in NOWAIT mode!

    t1=py_dt.datetime.now()
    required_concurrent_found = None
    concurrent_tx_pattern = re.compile('concurrent transaction number is \\d+', re.IGNORECASE)
    while True:
        concurrent_tx_number = None
        concurrent_runsql = ''
        tx_monitoring.begin()
        try:
            cur_monitoring.execute(sql_to_lock_record)
        except DatabaseError as exc:
            # Failed: SQL execution failed with: deadlock
            # -update conflicts with concurrent update
            # -concurrent transaction number is 40
            m = concurrent_tx_pattern.search( str(exc) )
            if m:
                concurrent_tx_number = m.group().split()[-1] # 'concurrent transaction number is 40' ==> '40'
                cur_monitoring.execute( 'select mon$sql_text from mon$statements where mon$transaction_id = ?', (int(concurrent_tx_number),) )
                for r in cur_monitoring:
                    concurrent_runsql = r[0]
                    if SQL_TAG_THAT_WE_WAITING_FOR in concurrent_runsql:
                        required_concurrent_found = 1

            # pytest.fail(f"Can not upd, concurrent TX = {concurrent_tx_number}, sql: {concurrent_runsql}")
        finally:
            tx_monitoring.rollback()
        
        if not required_concurrent_found:
            t2=py_dt.datetime.now()
            d1=t2-t1
            if d1.seconds*1000 + d1.microseconds//1000 >= MAX_WAIT_FOR_WORKER_START_MS:
                break
            else:
                time.sleep(0.2)
        else:
            break

    assert required_concurrent_found, f"Could not find attach that running SQL with tag '{SQL_TAG_THAT_WE_WAITING_FOR}' and locks record for {MAX_WAIT_FOR_WORKER_START_MS} ms. Check query: {sql_to_lock_record}. ABEND."
    return

#-----------------------------------------------------------------------------------------------------------------------------------------------------

@pytest.mark.trace
@pytest.mark.version('>=4.0.2')
def test_1(act: Action, fn_worker_sql: Path, fn_worker_log: Path, fn_worker_err: Path, capsys):
    sql_init = (act.files_dir / 'read-consist-sttm-restart-DDL.sql').read_text()

    for checked_mode in('table', 'view'):
        target_obj = 'test' if checked_mode == 'table' else 'v_test'

        SQL_TO_BE_RESTARTED = f"""
            merge /* {SQL_TAG_THAT_WE_WAITING_FOR} */ into {target_obj} t
            using(select * from {target_obj} where id >= 3 order by id) s on t.id = s.id
            when matched then
               DELETE
        """

        # add rows to test with ID = 2, 3, 5; add several child rows into detl.
        sql_addi='''
            recreate table detl(id int, PID int references test on delete cascade on update cascade);
            commit;

            delete from test;
            insert into test(id, x) values(2,2);
            insert into test(id, x) values(3,3);
            insert into test(id, x) values(5,5);
            insert into detl(id, pid) values(2000, 2);
            insert into detl(id, pid) values(2001, 2);
            insert into detl(id, pid) values(2002, 2);
            insert into detl(id, pid) values(3001, 3);
            insert into detl(id, pid) values(5001, 5);
            insert into detl(id, pid) values(5001, 5);
            commit;
        '''

        act.isql(switches=['-q', '-e'], input = 'recreate table detl(id int);', combine_output = True ) # drop dependencies
        if act.return_code or 'SQLSTATE =' in act.stdout:
            print(f"Could not delete table 'DETL', {act.return_code=} for {checked_mode=}:\n\n" + act.stdout)
            assert capsys.readouterr().out == ''
        act.reset()
       
        # NB: add switch '-e' in order to see entire executed code if script fails:
        act.isql(switches=['-q', '-e'], input = ''.join( (sql_init, sql_addi) ), combine_output = True )
        # ::: NOTE ::: We have to immediately quit if any error raised in prepare phase.
        # See also letter from dimitr, 01-feb-2022 14:46
        if act.return_code or 'SQLSTATE =' in act.stdout:
            print(f'Initial script failed with {act.return_code=} for {checked_mode=}:\n\n' + act.stdout)
            assert capsys.readouterr().out == ''
        act.reset()


        trace_cfg_items = [
            'time_threshold = 0',
            'log_errors = true',
            'log_statement_start = true',
            'log_statement_finish = true',
        ]

        with act.trace(db_events = trace_cfg_items, encoding=locale.getpreferredencoding()):

            with act.db.connect() as con_lock_1, act.db.connect() as con_lock_2, act.db.connect() as con_monitoring:

                tpb_monitoring = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0)
                tx_monitoring = con_monitoring.transaction_manager(tpb_monitoring)
                cur_monitoring = tx_monitoring.cursor()

                for i,c in enumerate((con_lock_1,con_lock_2)):
                    sttm = f"execute block as begin rdb$set_context('USER_SESSION', 'WHO', 'LOCKER #{i+1}'); end"
                    c.execute_immediate(sttm)

                #########################
                ###  L O C K E R - 1  ###
                #########################

                con_lock_1.execute_immediate( f'update {target_obj} set id=id where id=5' )

                worker_sql = f'''
                    set list on;
                    set autoddl off;
                    set term ^;
                    execute block returns (whoami varchar(30)) as
                    begin
                        whoami = 'WORKER'; -- , ATT#' || current_connection;
                        rdb$set_context('USER_SESSION','WHO', whoami);
                        -- suspend;
                    end
                    ^
                    set term ;^
                    commit;
                    SET KEEP_TRAN_PARAMS ON;
                    set transaction read committed read consistency;
                    set list off;
                    set wng off;
                    set count on;

                    -- MUST HANG:
                    {SQL_TO_BE_RESTARTED};

                    -- check results:
                    -- ###############
                    select id from {target_obj} order by id; -- this will produce output only after all lockers do their commit/rollback

                    select v.old_id, v.op, v.snap_no_rank
                    from v_worker_log v
                    where v.op = 'del';

                    rollback;

                '''

                fn_worker_sql.write_text(worker_sql)

                with fn_worker_log.open(mode='w') as hang_out, fn_worker_err.open(mode='w') as hang_err:

                    ############################################################################
                    ###  L A U N C H     W O R K E R    U S I N G     I S Q L,   A S Y N C.  ###
                    ############################################################################
                    p_worker = subprocess.Popen([act.vars['isql'], '-i', str(fn_worker_sql),
                                                   '-user', act.db.user,
                                                   '-password', act.db.password,
                                                   act.db.dsn
                                                ],
                                                  stdout = hang_out,
                                                  stderr = hang_err
                                               )
                    # NB: when ISQL will establish attach, first record that it must lock is ID = 3 -- see above SQL_TO_BE_RESTARTED
                    # We must to ensure that this (worker) attachment has been really created and LOCKS this record:
                    #
                    wait_for_record_become_locked(tx_monitoring, cur_monitoring, f'update {target_obj} set id=id where id=3', SQL_TAG_THAT_WE_WAITING_FOR)


                    #########################
                    ###  L O C K E R - 2  ###
                    #########################
                    con_lock_2.execute_immediate( f'update {target_obj} set id=4 where id=2;' )
                    con_lock_2.commit()
                    con_lock_2.execute_immediate( f'update {target_obj} set id=id where id=4;' )


                    #########################
                    ###  L O C K E R - 1  ###
                    #########################
                    con_lock_1.commit() # release record with ID=5 (allow it to be deleted by session-worker)

                    # We have to WAIT HERE until worker will actually 'catch' just released record with ID = 5.
                    #
                    wait_for_record_become_locked(tx_monitoring, cur_monitoring, f'update {target_obj} set id=id where id=5', SQL_TAG_THAT_WE_WAITING_FOR)
                    # If we come here then it means that record with ID = 5 for sure is locked by WORKER.

                    # Add record which did not exists when session-worker statement started.
                    # Add also child record for it, then commit + re-lock just added record:
                    con_lock_1.execute_immediate( f'insert into {target_obj}(id,x) values(6,6)' )
                    con_lock_1.execute_immediate( f'insert into detl(id, pid) values(6001, 6)' )
                    con_lock_1.commit()
                    con_lock_1.execute_immediate( f'update {target_obj} set id=id where id=6' )

                    #########################
                    ###  L O C K E R - 2  ###
                    #########################
                    con_lock_2.commit() # release record with ID=4. At this point session-worker will be allowed to delete rows with ID=4 and 5.

                    # We have to WAIT HERE until worker will actually 'catch' just released record with ID = 4.
                    #
                    wait_for_record_become_locked(tx_monitoring, cur_monitoring, f'update {target_obj} set id=id where id=4', SQL_TAG_THAT_WE_WAITING_FOR)
                    # If we come here then it means that record with ID = 4 for sure is locked by WORKER.

                    con_lock_1.commit() # release record with ID=6. It is the last record which also must be deleted by session-worker.

                    # Here we wait for ISQL complete its mission:
                    p_worker.wait()


            #< with act.db.connect()
                
            for g in (fn_worker_log, fn_worker_err):
                with g.open() as f:
                    for line in f:
                        if line.split():
                            if g == fn_worker_log:
                                print(f'checked_mode: {checked_mode}, STDLOG: {line}')
                            else:
                                print(f'UNEXPECTED STDERR {line}')

            expected_stdout_worker = f"""
                checked_mode: {checked_mode}, STDLOG: Records affected: 4

                checked_mode: {checked_mode}, STDLOG: Records affected: 0

                checked_mode: {checked_mode}, STDLOG:  OLD_ID OP              SNAP_NO_RANK
                checked_mode: {checked_mode}, STDLOG: ======= ====== =====================
                checked_mode: {checked_mode}, STDLOG:       3 DEL                        1
                checked_mode: {checked_mode}, STDLOG:       3 DEL                        2
                checked_mode: {checked_mode}, STDLOG:       3 DEL                        3
                checked_mode: {checked_mode}, STDLOG:       4 DEL                        3
                checked_mode: {checked_mode}, STDLOG:       5 DEL                        3
                checked_mode: {checked_mode}, STDLOG:       6 DEL                        3

                checked_mode: {checked_mode}, STDLOG: Records affected: 6
            """

            act.expected_stdout = expected_stdout_worker
            act.stdout = capsys.readouterr().out
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()

        # < with act.trace

        allowed_patterns = \
        [
             '\\)\\s+EXECUTE_STATEMENT_RESTART$'
            ,'^Restarted \\d+ time\\(s\\)'
        ]
        allowed_patterns = [re.compile(x) for x in allowed_patterns]

        for line in act.trace_log:
            if line.strip():
                if act.match_any(line.strip(), allowed_patterns):
                    print(line.strip())

        expected_stdout_trace = f"""
            EXECUTE_STATEMENT_RESTART
            Restarted 1 time(s)

            EXECUTE_STATEMENT_RESTART
            Restarted 2 time(s)
        """
                
        act.expected_stdout = expected_stdout_trace
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

    # < for checked_mode
