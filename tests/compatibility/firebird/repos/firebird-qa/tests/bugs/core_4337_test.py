#coding:utf-8

"""
ID:          issue-1535
ISSUE:       1535
TITLE:       gfix -sweep makes "reconnect" when it is removed from mon$attachments by delete command (issued in another window)
DESCRIPTION:
    We create table with several very wide char fields and fill it with uuid data.

    Then we create indexes for these fields and finally - delete all rows.
    Such table will require valuable time to be swept, about 4..5 seconds
    (we set DB forced_writes = ON and small buffers number in the DB header).

    After this we launch trace and 'gfix -sweep' (asynchronously, via subprocess.Popen()).
    Then we immediately open cursor and start LOOP with query to mon$attachments.
    Loop will work until connection created by gfix will be seen there (usually this must occurs instantly).

    When gfix connection is known, we execute 'DELETE FROM MON$ATTACHMENT' command which should kill its attachment.
    Process of GFIX should raise error 'connection shutdown' - and we check this by saving its output to log.

    Further, we have to REPEAT check of mon$attachments after this and connection by 'gfix' must NOT present there.

    Then we stop trace and check its log. It must contain line with text 'SWEEP_FAILED' and after this line
    we should NOT discover any lines with 'ATTACH_DATABASE' event - this is especially verified.

    Finally, we compare content of firebird.log: new lines in it should contain messages about
    interrupted sweep ('error during sweep' and 'connection shutdown').
JIRA:        CORE-4337
FBTEST:      bugs.core_4337
NOTES:
    [26.09.2017]
        adjusted expected_stdout section to current FB 3.0+ versions: firebird.log now does contain info
        about DB name which was swept (see CORE-5610, "Provide info about database (or alias) which was in use...")
    [19.11.2021] pcisar
        Small difference in reimplementation of sweep killer script
        On v4.0.0.2496 COMMIT after delete from mon#attachments fails with:
        STATEMENT FAILED, SQLSTATE = HY008, OPERATION WAS CANCELLED
        Without commit this test PASSES, i.e. sweep is terminated with all outputs as expected
    [14.09.2022] pzotov
        Re-implemented: avoid usage of ISQL with fragile timeout setting (gfix can complete FASTER than isql will see it!).
        Appearance of 'gfix' process is checked in loop by querying mon$attachments, see also: MAX_WAIT_FOR_SWEEP_START_MS.
        More precise pattern for line with message about failed sweep (see 'p_sweep_failed').
        Checked on Windows and Linux: 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.730
    [30.07.2025] pzotov
        Re-implemented: in case of failed assumption about some intermediate result, we have to 'accumulate' output and print
        it at final point of this test (instead of break on assertion). If 'Unexpected p_sweep.returncode=0' will present in
        the output than we have to increase field length and/or rows count (because sweep complets its job faster than we can
        to establish connection for check presense of gfix in mon$attachments).
    Checked on Windows: 6.0.0.1092; 5.0.3.1689; 4.0.6.3223; 3.0.13.33818.
"""

import datetime as py_dt
from datetime import timedelta
import time
import subprocess
from difflib import unified_diff
import re
from pathlib import Path
from firebird.driver import DbWriteMode, DatabaseError

from firebird.qa import *
import pytest

substitutions = [
                  ('[ ]+', ' '),
                  ('TRACE_LOG:.* SWEEP_START', 'TRACE_LOG: SWEEP_START'),
                  ('TRACE_LOG:.* SWEEP_FAILED', 'TRACE_LOG: SWEEP_FAILED'),
                  ('TRACE_LOG:.* ERROR AT JPROVIDER::ATTACHDATABASE', 'TRACE_LOG: ERROR AT JPROVIDER::ATTACHDATABASE'),
                  ('.*KILLED BY DATABASE ADMINISTRATOR.*', ''),
                  ('TRACE_LOG:.*GFIX.EXE.*', 'TRACE_LOG: GFIX.EXE'),
                  ('OIT [0-9]+', 'OIT'), ('OAT [0-9]+', 'OAT'), ('OST [0-9]+', 'OST'),
                  ('NEXT [0-9]+', 'NEXT'),
                  ('FIREBIRD.LOG:.* ERROR DURING SWEEP OF .*TEST.FDB.*', 'FIREBIRD.LOG: + ERROR DURING SWEEP OF TEST.FDB')
                ]

###########################
###   S E T T I N G S   ###
###########################

# ::: NB ::: Increase field length and/or rows count if sweep will complete its work
# before we establish connection to check presense of gfix in mon$attachments:
#
FIELD_LEN = 4000
ROWS_CNT = 25000

# How long can we wait (milliseconds) for the FIRST appearance of 'gfix' process in mon$attachments:
#
MAX_WAIT_FOR_SWEEP_START_MS = 3000

# How long we wait (milliseconds) for SECOND appearance of 'gfix' process in mon$attachments, after it was killed.
# NOTE. If it appears then we have a BUG
#
MAX_WAIT_FOR_GFIX_RESTART_MS = 3000

FOUND_FAILED_SWEEP_MSG = 'FOUND SWEEP_FAILED MESSAGE.'
############################

init_sql = f"""
    set list on;
    recreate table t(
         s01 varchar({FIELD_LEN})
        ,s02 varchar({FIELD_LEN})
        ,s03 varchar({FIELD_LEN})
        ,s04 varchar({FIELD_LEN})
    );
    commit;
    set term ^;
    execute block as
        declare n int = {ROWS_CNT};
        declare w int;
    begin
        select f.rdb$field_length
        from rdb$relation_fields rf
        join rdb$fields f on rf.rdb$field_source=f.rdb$field_name
        where rf.rdb$relation_name=upper('t') and rf.rdb$field_name=upper('s01')
        into w;

        while (n>0) do
            insert into t(s01, s02, s03, s04)
            values(
                       rpad('', :w, uuid_to_char(gen_uuid())) 
                      ,rpad('', :w, uuid_to_char(gen_uuid())) 
                      ,rpad('', :w, uuid_to_char(gen_uuid())) 
                      ,rpad('', :w, uuid_to_char(gen_uuid())) 
                  ) 
            returning :n-1 into n
        ;
    end^
    set term ;^
    commit;
    create index t_s01 on t(s01);
    create index t_s02 on t(s02);
    create index t_s03 on t(s03);
    create index t_s04 on t(s04);
    commit;

    delete from t;
    commit;
    quit;
"""

# 16K page is neccessary ! Smaller size will cause failure with:
# key size exceeds implementation restriction for index "T_S01"
#
db = db_factory(page_size=16384, init = init_sql)

act = python_act('db', substitutions = substitutions)

sweep_log = temp_file('tmp_4337_sweep.log')

trace = ['time_threshold = 0',
         'log_errors = true',
         'log_sweep = true',
         'log_connections = true',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action, sweep_log: Path, capsys):

    with act.connect_server() as srv:
        # REDUCE number of cache buffers in DB header in order to sweep make its work as long as possible
        # Attempt to set too low value will fail with:
        # bad parameters on attach or create database
        # -Attempt to set in database number of buffers which is out of acceptable range [50:2147483646]
        srv.database.set_default_cache_size(database=act.db.db_path, size = 75)

        # Change FW to ON (in order to make sweep life harder :))
        srv.database.set_write_mode(database=act.db.db_path, mode=DbWriteMode.SYNC)

        srv.info.get_log()
        log_before = srv.readlines()

    #---------------------------------------------------------------

    sweep_attach_id = None
    sweep_reconnect = None
    # Start trace
    with act.trace(db_events=trace):
        with act.db.connect() as con:
            f_sweep_log = open(sweep_log,'w')
            stm = r"select first 1 a.mon$attachment_id from mon$attachments a where a.mon$system_flag <> 1 and lower(a.mon$remote_process) similar to '(%[\\/](gfix|fbsvcmgr)(.exe)?)'"
            t1=py_dt.datetime.now()
            with con.cursor() as cur1:
                ps1, rs1 = None, None
                try:
                    p_sweep = subprocess.Popen( [act.vars['gfix'], '-sweep', '-user', act.db.user, '-password', act.db.password, act.db.dsn],
                                                stdout = f_sweep_log,
                                                stderr = subprocess.STDOUT
                                              )

                    ##########################################################################
                    # LOOP-1: WAIT FOR FIRST APPEARANCE OF GFIX PROCESS IN THE MON$ATTACHMENTS
                    ##########################################################################
                    ps1 = cur1.prepare(stm)
                    while True:
                        t2=py_dt.datetime.now()
                        d1=t2-t1
                        dd = d1.seconds*1000 + d1.microseconds//1000
                        if dd > MAX_WAIT_FOR_SWEEP_START_MS:
                            print(f'TIMEOUT EXPIRATION: waiting for SWEEP process took {dd} ms which exceeds limit = {MAX_WAIT_FOR_SWEEP_START_MS} ms.')
                            break

                        # ::: NB ::: 'ps1' returns data, i.e. this is SELECTABLE expression.
                        # We have to store result of cur.execute(<psInstance>) in order to
                        # close it explicitly.
                        # Otherwise AV can occur during Python garbage collection and this
                        # causes pytest to hang on its final point.
                        # Explained by hvlad, email 26.10.24 17:42
                        rs1 = cur1.execute(ps1)
                        for r in cur1:
                            sweep_attach_id = r[0]
                        
                        con.commit()
                        if sweep_attach_id:
                            break
                        else:
                            time.sleep(0.1)
                    #<while True (loop for search gfix process in mon$attachments)
    
                except DatabaseError as e:
                    print( e.__str__() )
                    print(e.gds_codes)
                finally:
                    if rs1:
                        rs1.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                    if ps1:
                        ps1.free()


            #< with con.cursor() as cur1

            #assert sweep_attach_id is None, f'attacment_id of SWEEP process is {sweep_attach_id}'
            assert sweep_attach_id, 'attacment_id of SWEEP process was not found.'

            # Let sweep to have some 'useful work':
            #
            time.sleep(1)

            # Now we can KILL attachment that belongs to SWEEP process, <sweep_attach_id>:
            sweep_kill_beg = py_dt.datetime.now()
            con.execute_immediate(f'delete from mon$attachments where mon$attachment_id = {sweep_attach_id}')
            sweep_kill_end = py_dt.datetime.now()

            p_sweep.wait()
            f_sweep_log.close()

            if p_sweep.returncode == 1:
                ##################################################################################################
                # LOOP-2: WAIT FOR POSSIBLE SECOND APPEARENCE (RECONNECT) OF GFIX. IF IT OCCURS THEN WE HAVE A BUG
                ##################################################################################################
                t1=py_dt.datetime.now()
                with con.cursor() as cur2:
                    ps2, rs2 = None, None
                    try:
                        ps2 = cur2.prepare( stm.replace('select ', 'select /* search re-connect that could be made */ ') )
                        while True:
                            t2=py_dt.datetime.now()
                            d1=t2-t1
                            dd = d1.seconds*1000 + d1.microseconds//1000
                            if dd > MAX_WAIT_FOR_GFIX_RESTART_MS:
                                # Expected: gfix reconnect was not detected for last {MAX_WAIT_FOR_GFIX_RESTART_MS} ms.
                                break
                            con.commit()
                            rs2 = cur2.execute(ps2)
                            # Resultset now must be EMPTY. we must not find any record!
                            for r in cur2:
                                sweep_reconnect = r[0]
                            
                            #con.commit()
                            if sweep_reconnect:
                                # UNEXPECTED: gfix reconnect found, with attach_id={sweep_reconnect}
                                break
                            else:
                                time.sleep(0.1)

                    except DatabaseError as e:
                        print( e.__str__() )
                        print(e.gds_codes)
                    finally:
                        if rs2:
                            rs2.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                        if ps2:
                            ps2.free()
                
                #< with con.cursor() as cur2
            else:
                # Test should be considered as FAILED. Make additional output for debug:
                print(f'Unexpected {p_sweep.returncode=}. Timestamps: {sweep_kill_beg=}, {sweep_kill_end=} (compare with firebird.log if needed)')

            if sweep_reconnect:
                # Test should be considered as FAILED. Make additional output for debug:
                print(f'Unexpected re-connect of SWEEP process, attachment: {sweep_reconnect}')

        #< with db.connect as con

    #---------------------------------------------------------------
    # DISABLED 25.09.2022. SWEEP log can remain empty (4.0.1.2692 Classic)
    ## Check-1: log of sweep must contain text: 'connection shutdown':
    #for line in sweep_log.read_text().splitlines():
    #    if line:
    #        print('SWEEP LOG:', line.upper())
    #
    #act.expected_stdout = 'sweep log: connection shutdown'.upper()
    #act.stdout = capsys.readouterr().out
    #assert act.clean_stdout == act.clean_expected_stdout
    #act.reset()
    #---------------------------------------------------------------

    # Trace log
    found_sweep_failed = 0
    p_sweep_failed = re.compile( r'[.*\s+]*20\d{2}(-\d{2}){2}T\d{2}(:\d{2}){2}.\d{3,4}\s+\(.+\)\s+SWEEP_FAILED$', re.IGNORECASE)
    p_att_success = re.compile( r'[.*\s+]*20\d{2}(-\d{2}){2}T\d{2}(:\d{2}){2}.\d{3,4}\s+\(.+\)\s+ATTACH_DATABASE$', re.IGNORECASE)

    for i,line in enumerate(act.trace_log):
        if line.strip():
            if p_sweep_failed.search(line.strip()):
                print(FOUND_FAILED_SWEEP_MSG)
                found_sweep_failed = 1
            if found_sweep_failed == 1 and p_att_success.search(line) and i < len(act.trace_log)-2 and 'gfix' in act.trace_log[i+2].lower():
                # NB: we have to ignore "FAILED ATTACH_DATABASE".
                # It can occur in some cases.
                print('TRACE: UNEXPECTED ATTACH FOUND AFTER KILL SWEEP! CHECK LINE N {i}:')
                print('TRACE_LOG: ' + line)

    if not found_sweep_failed:
        # Test should be considered as FAILED. Make additional output for debug:
        print('Trace log: could not find message about FAILED sweep:')
        print('--- start of trace log ---')
        for i,line in enumerate(act.trace_log):
            if line.strip():
                print(line)
        print('--- finish of trace log ---')

    #act.expected_stdout = FOUND_FAILED_SWEEP_MSG
    #act.stdout = capsys.readouterr().out
    #assert act.clean_stdout == act.clean_expected_stdout
    #act.reset()

    #----------------------------------------------------------------

    with act.connect_server() as srv:
        srv.info.get_log()
        log_after = srv.readlines()

    '''
      Example of diff:
      COMPUTERNAME	Wed Sep 14 15:58:37 2022
      	Sweep is started by SYSDBA
      	Database "C:/TEMP/PYTEST_PATH/TEST.FDB" 
      	OIT 20, OAT 21, OST 21, Next 21

      COMPUTERNAME	Wed Sep 14 15:58:37 2022
      	Error during sweep of C:/PYTEST_PATH/TEST.FDB:
      	connection shutdown
    '''

    p_tx_counter  = re.compile("\\+[\\s]+OIT[ ]+\\d+,[\\s]*OAT[\\s]+\\d+,[\\s]*OST[\\s]+\\d+,[\\s]*NEXT[\\s]+\\d+")
    for line in unified_diff(log_before, log_after):
        if line.startswith('+'):
            line = line.upper()
            if 'SWEEP' in line or 'CONNECTION' in line or p_tx_counter.match(line):
                print( 'FIREBIRD.LOG: ' + (' '.join(line.split())) )

    fb_log_expected = f"""
        {FOUND_FAILED_SWEEP_MSG}
        FIREBIRD.LOG: + SWEEP IS STARTED BY SYSDBA
        FIREBIRD.LOG: + OIT, OAT, OST, NEXT
        FIREBIRD.LOG: + ERROR DURING SWEEP OF TEST.FDB
        FIREBIRD.LOG: + CONNECTION SHUTDOWN
    """

    act.expected_stdout = fb_log_expected
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
