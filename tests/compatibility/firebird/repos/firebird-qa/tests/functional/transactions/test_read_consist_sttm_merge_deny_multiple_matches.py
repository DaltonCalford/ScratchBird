#coding:utf-8

"""
ID:          transactions.read-consist-sttm-merge-deny-multiple-matches
TITLE:       READ CONSISTENCY. MERGE must reject multiple matches, regardless on statement-level restart.
DESCRIPTION:
  Initial article for reading:
      https://asktom.oracle.com/pls/asktom/f?p=100:11:::::P11_QUESTION_ID:11504247549852
      Note on terms which are used there: "BLOCKER", "LONG" and "FIRSTLAST" - their names are slightly changed here
      to: LOCKER-1, WORKER and LOCKER-2 respectively.

      See also: doc/README.read_consistency.md
      Letter from Vlad: 15.09.2020 20:04 // subj "read consistency // addi test(s)"

      ::: NB :::
      This test uses script %FBT_REPO%/files/read-consist-sttm-restart-DDL.sql which contains common DDL for all other such tests.
      Particularly, it contains two TRIGGERS (TLOG_WANT and TLOG_DONE) which are used for logging of planned actions and actual
      results against table TEST. These triggers use AUTONOMOUS transactions in order to have ability to see results in any
      outcome of test.

      Test verifies DENIAL of multiple matches when MERGE encounteres them, but this statement works in read committed read consistency TIL
      and forced to do several statement-level restarts before such condition will occur.

      Scenario:
      * add initial data to the table TEST: six rows with ID and X = (0, ..., 5);
      * launch LOCKER-1 and catch record with ID = 0: update, then commit and once again update this record (without commit);
      * launch WORKER which tries to do:
            merge into test t
                using (
                    select s.id, s.x from test as s
                    where s.id <= 1
                    order by s.id DESC
                ) s
                on abs(t.id) = abs(s.id)
            when matched then
                update set t.x = t.x * s.x
            ;
        This statement will update record with ID = 1 but then hanging because rows with ID = 0 is locked by LOCKER-1.
        At this point WORKER changes its TIL to RC NO RECORD_VERSION. This allows WORKER to see all records which will be committed later;
        NOTE: records with ID = 2...5 will not be subect for this statement (because they will not returned by data source marked as 's').

      * LOCKER-2 updates row with ID = 5 by reverting sign of this field (i.e. set ID to -5), then issues commit and updates this row again (without commit);
      * LOCKER-1 updates row with ID = 4 and set ID to -4, then issues commit and updates this row again (without commit);
      * LOCKER-2 updates row with ID = 3 and set ID to -3, then issues commit and updates this row again (without commit);
      * LOCKER-1 updates row with ID = 2 and set ID to -2, then issues commit and updates this row again (without commit);
      * LOCKER-2 inserts row (ID,X) = (-1, 1), commit and updates this row again (without commit);
      * LOCKER-1 issues commit;
      * LOCKER-2 issues commit;

      Each of updates/inserts which are performed by LOCKERs lead to new record be appeared in the data source 's' of MERGE statement.
      But note that last statement: insert into test(id,x) values(-1,1) -- creates record that will match TWISE when ON-expression of MERGE
      will evaluates "abs(t.id) = abs(s.id)": first match will be found for record with ID = +1 and second - for newly added rows with ID=-1.

      At this point MERGE must fail with message
          Statement failed, SQLSTATE = 21000
          Multiple source records cannot match the same target during MERGE

      All changes that was performed by MERGE must be rolled back.
      ISQL which did MERGE must issue "Records affected: 2" because MERGE was actually could process records with ID = 1 and 0 (and failed on row with ID=-1).

      Above mentioned actions are performed two times: first for TABLE and second for naturally-updatable VIEW (v_test), see 'target_object_type'.

FBTEST:      functional.transactions.read_consist_sttm_merge_deny_multiple_matches
NOTES:
    [29.07.2022] pzotov
        Checked on 4.0.1.2692, 5.0.0.591
    [27.09.2023] pzotov
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

        Checked on WI-T6.0.0.55, WI-T5.0.0.1229, WI-V4.0.4.2995 (all SS/CS).

    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
    Merge expression defined in 'SQL_TO_BE_RESTARTED' must NOT end with semicolon!
"""

import subprocess
import re
from difflib import unified_diff
from pathlib import Path
import time
import datetime as py_dt
import locale

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, TraAccessMode, DatabaseError

db = db_factory()

act = python_act( 'db', substitutions = [ ('=', ''), ('[ \t]+', ' '), ('.* EXECUTE_STATEMENT_RESTART', 'EXECUTE_STATEMENT_RESTART'), ('(At|After) line .*', '') ] )

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
                using (
                    select s.id, s.x from {target_obj} as s
                    where s.id <= 1
                    order by s.id DESC -- added only 05-jun-2024; thanks to Vlad.
                ) s
                on abs(t.id) = abs(s.id)
            when matched then
                update set t.x = s.id * 100
        """

        sql_addi = f'''
            set term ^;
            execute block as
            begin
                rdb$set_context('USER_SESSION', 'WHO', 'INIT_DATA');
            end
            ^
            set term ;^

             -- INITIAL DATA: add rows with ID = 0...6
             -- #############
            insert into {target_obj}(id, x)
            select row_number()over()-1, row_number()over()-1
            from rdb$types rows 6;

            commit;
        '''

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

                cur_lock_1 = con_lock_1.cursor()
                cur_lock_2 = con_lock_2.cursor()

                for i,c in enumerate((con_lock_1,con_lock_2)):
                    sttm = f"execute block as begin rdb$set_context('USER_SESSION', 'WHO', 'LOCKER #{i+1}'); end"
                    c.execute_immediate(sttm)


                #########################
                ###  L O C K E R - 1  ###
                #########################

                con_lock_1.execute_immediate( f'update {target_obj} set id=id where id = 0' )

                worker_sql = f'''
                    set list on;
                    set autoddl off;
                    set term ^;
                    execute block as
                    begin
                        rdb$set_context('USER_SESSION','WHO', 'WORKER');
                    end
                    ^
                    set term ;^
                    commit;
                    SET KEEP_TRAN_PARAMS ON;
                    set transaction read committed read consistency;
                    set list off;
                    set wng off;

                    set count on;
                    -- THIS MUST HANG:
                    {SQL_TO_BE_RESTARTED};

                    -- check results:
                    -- ###############
                    select id,x from {target_obj} order by id;

                    select v.old_id, v.op, v.snap_no_rank
                    from v_worker_log v
                    where v.op = 'upd';


                    --set width who 10;
                    -- DO NOT check this! Values can differ here from one run to another!
                    -- select id, trn, who, old_id, new_id, op, rec_vers, global_cn, snap_no from tlog_done order by id;
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
                                                   '-pag', '999999',
                                                   act.db.dsn
                                                ],
                                                  stdout = hang_out,
                                                  stderr = hang_err
                                               )

                    # NB: when ISQL will establish attach, first record that it must lock is ID = 0 -- see above SQL_TO_BE_RESTARTED
                    # We must to ensure that this (worker) attachment has been really created and LOCKS this record:
                    #
                    #wait_for_record_become_locked(tx_monitoring, cur_monitoring, f'update {target_obj} set id=id where id = 0', SQL_TAG_THAT_WE_WAITING_FOR)

                    sttm = f'update {target_obj} set id = ? where abs( id ) = ?'

                    #########################
                    ###  L O C K E R - 2  ###
                    #########################
                    cur_lock_2.execute( sttm, ( -5, 5, ) )
                    con_lock_2.commit()
                    cur_lock_2.execute( sttm, ( -5, 5, ) )

                    #########################
                    ###  L O C K E R - 1  ###
                    #########################
                    con_lock_1.commit() # releases record with ID = 0 ==> now it can be locked by worker.
                    # We must to ensure that this (worker) attachment has been really created and LOCKS this record:
                    #
                    wait_for_record_become_locked(tx_monitoring, cur_monitoring, f'update {target_obj} set id=id where id=0', SQL_TAG_THAT_WE_WAITING_FOR)


                    cur_lock_1.execute( sttm, ( -4, 4, ) )
                    con_lock_1.commit()
                    cur_lock_1.execute( sttm, ( -4, 4, ) )

                    #########################
                    ###  L O C K E R - 2  ###
                    #########################
                    con_lock_2.commit() # releases record with ID = -5 ==> now it can be locked by worker.
                    # We must to ensure that this (worker) attachment has been really created and LOCKS this record:
                    #
                    wait_for_record_become_locked(tx_monitoring, cur_monitoring, f'update {target_obj} set id=id where id=-5', SQL_TAG_THAT_WE_WAITING_FOR)

                    cur_lock_2.execute( sttm, ( -3, 3, ) )
                    con_lock_2.commit()
                    cur_lock_2.execute( sttm, ( -3, 3, ) )

                    #########################
                    ###  L O C K E R - 1  ###
                    #########################
                    con_lock_1.commit()
                    # We must to ensure that this (worker) attachment has been really created and LOCKS this record:
                    #
                    wait_for_record_become_locked(tx_monitoring, cur_monitoring, f'update {target_obj} set id=id where id=-4', SQL_TAG_THAT_WE_WAITING_FOR)

                    cur_lock_1.execute( sttm, ( -2, 2, ) )
                    con_lock_1.commit()
                    cur_lock_1.execute( sttm, ( -2, 2, ) )

                    #########################
                    ###  L O C K E R - 2  ###
                    #########################
                    con_lock_2.commit()
                    # We must to ensure that this (worker) attachment has been really created and LOCKS this record:
                    #
                    wait_for_record_become_locked(tx_monitoring, cur_monitoring, f'update {target_obj} set id=id where id=-3', SQL_TAG_THAT_WE_WAITING_FOR)


                    cur_lock_2.execute( f'insert into {target_obj}(id,x) values(?, ?)', ( -1, 1, ) )
                    con_lock_2.commit()
                    cur_lock_2.execute( f'update {target_obj} set id = id where id = ?', ( -1, ) )

                    #########################
                    ###  L O C K E R - 1  ###
                    #########################
                    con_lock_1.commit()
                    # We must to ensure that this (worker) attachment has been really created and LOCKS this record:
                    #
                    wait_for_record_become_locked(tx_monitoring, cur_monitoring, f'update {target_obj} set id=id where id=-2', SQL_TAG_THAT_WE_WAITING_FOR)

                    #########################
                    ###  L O C K E R - 2  ###
                    #########################
                    con_lock_2.commit() # At this point merge can complete its job but it must FAIL because of multiple matches for abs(t.id) = abs(s.id), i.e. when ID = -1 and 1

                    # Here we wait for ISQL complete its mission:
                    p_worker.wait()

            # < with act.db.connect

            for g in (fn_worker_log, fn_worker_err):
                with g.open() as f:
                    for line in f:
                        if line.strip():
                            print(f'checked_mode: {checked_mode}, {"STDLOG" if g == fn_worker_log else "STDERR"}: {line}')

            expected_stdout_worker = f"""
                checked_mode: {checked_mode}, STDLOG: Records affected: 2
                checked_mode: {checked_mode}, STDLOG:      ID       X
                checked_mode: {checked_mode}, STDLOG: ======= =======
                checked_mode: {checked_mode}, STDLOG:      -5       5
                checked_mode: {checked_mode}, STDLOG:      -4       4
                checked_mode: {checked_mode}, STDLOG:      -3       3
                checked_mode: {checked_mode}, STDLOG:      -2       2
                checked_mode: {checked_mode}, STDLOG:      -1       1
                checked_mode: {checked_mode}, STDLOG:       0       0
                checked_mode: {checked_mode}, STDLOG:       1       1
                checked_mode: {checked_mode}, STDLOG: Records affected: 7
                checked_mode: {checked_mode}, STDLOG:  OLD_ID OP              SNAP_NO_RANK
                checked_mode: {checked_mode}, STDLOG: ======= ====== =====================
                checked_mode: {checked_mode}, STDLOG:       0 UPD                        1
                checked_mode: {checked_mode}, STDLOG:       1 UPD                        1
                checked_mode: {checked_mode}, STDLOG:       0 UPD                        2
                checked_mode: {checked_mode}, STDLOG:       1 UPD                        2
                checked_mode: {checked_mode}, STDLOG:       0 UPD                        3
                checked_mode: {checked_mode}, STDLOG:       1 UPD                        3
                checked_mode: {checked_mode}, STDLOG:       0 UPD                        4
                checked_mode: {checked_mode}, STDLOG:       1 UPD                        4
                checked_mode: {checked_mode}, STDLOG:       0 UPD                        5
                checked_mode: {checked_mode}, STDLOG:       1 UPD                        5
                checked_mode: {checked_mode}, STDLOG: Records affected: 10
                checked_mode: {checked_mode}, STDERR: Statement failed, SQLSTATE = 21000
                checked_mode: {checked_mode}, STDERR: Multiple source records cannot match the same target during MERGE
                checked_mode: {checked_mode}, STDERR:
            """

            act.expected_stdout = expected_stdout_worker
            act.stdout = capsys.readouterr().out
            assert act.clean_stdout == act.clean_expected_stdout
            act.reset()

        # < with act.trace

        allowed_patterns = \
        [
             '\\)\\s+EXECUTE_STATEMENT_RESTART$'
            #,re.escape(SQL_TO_BE_RESTARTED)
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

            EXECUTE_STATEMENT_RESTART
            Restarted 3 time(s)

            EXECUTE_STATEMENT_RESTART
            Restarted 4 time(s)
        """
                
        act.expected_stdout = expected_stdout_trace
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
        act.reset()

    # < for checked_mode
