#coding:utf-8

"""
ID:          issue-3690
ISSUE:       3690
TITLE:       Ability to cancel waiting in lock manager
DESCRIPTION:
    We create table and add <CONCURRENT_ATT_CNT> rows in it (which equals to number of concurrent ISQL workers that are launched).
    Record with ID = 0 is locked by 'main code' i.e. not by ISQL.
    Then we start asynchronously <CONCURRENT_ATT_CNT> ISQL workers (via subprocess.Popen()), but after each worker launch we WAIT for
    its data (executing statement) appear in the mon$statements table (we do not go on until just launched ISQL appear in mon$ table!).
    After appearing in mon$statements data of this worker, we can repeat with next worker, etc.

    Worker #1 locks record with ID = 1 and attempts to lock record with ID = 0 (and falls into infinite wait because of 'main code' lock).
    Worker #2 locks record with ID = 2 and attempts to lock record with ID = 1 (and also falls into infinite wait because of ISQL worker #1).
    Last worker tries to lock record with ID = 0 - and thus all of them are in infinite pause.
    ID of each ISQL attachment is added to the list in order to kill these attachments.

    Then we walk through the  list of attachment IDs in reversed order and issue 'delete from mon$attachments' for each of them.
    Finally, we check that:
        * every ISQL worker must be terminated with returncode = 1;
        * every worker log must contain text 'SQLSTATE = 08003';
        * no alive ISQL processes remain after issuing 'delete from mon$..' statements.
JIRA:        CORE-3323
NOTES:
    [17.11.2021] pcisar
        This test is too complicated and fragile (can screw the test environment)
        It should be reimplemented in more robust way, or removed from suite

    [19.09.2022] pzotov
    Fully re-implemented 19.09.2022 (this is second attempt to force this test work with predict outcome; fingers crossed... ;)).
    No more any dependency on any host and/or environment properties (like poor performance or hardware etc), with exception of case
    when extremely high workload exists and we can not even establish connection to test database (see 'MAX_WAIT_FOR_ISQL_START_MS')
    or some trouble occurs with deleting from mon$attachments.

    Checked on 3.0.8.33535 (SS/CS), 4.0.1.2692 (SS/CS), 5.0.0.730 (SS/CS) - both Linux and Windows.

    [18.01.2025] pzotov
    Resultset of cursor that executes using instance of selectable PreparedStatement must be stored
    in some variable in order to have ability close it EXPLICITLY (before PS will be freed).
    Otherwise access violation raises during Python GC and pytest hangs at final point (does not return control to OS).
    This occurs at least for: Python 3.11.2 / pytest: 7.4.4 / firebird.driver: 1.10.6 / Firebird.Qa: 0.19.3
    The reason of that was explained by Vlad, 26.10.24 17:42 ("oddities when use instances of selective statements").
"""
import time
import datetime as py_dt
import subprocess
import re

from typing import List
from pathlib import Path

import pytest
from firebird.qa import *
from firebird.driver import DatabaseError

###########################
###   S E T T I N G S   ###
###########################

# Number of launched ISQL sessions:
#
CONCURRENT_ATT_CNT = 50

# How long can we wait for ISQL be loaded and will
# appear in the FB monitoring tables, milliseconds.
# We check this using LOOP with query to mon$ statements:
#
MAX_WAIT_FOR_ISQL_START_MS = 3000

# How long can we wait until ISQL process will be SELF-COMPLETED
# because we kill its attachment (see 'p_isql.wait(...)'), seconds.
# See also: https://docs.python.org/3/library/subprocess.html#subprocess.Popen.wait
#
MAX_WAIT_FOR_ISQL_FINISH_S = 10

init_ddl = f"""
    recreate table test(id int primary key using descending index test_id_desc);
    set term ^;
    execute block as
        declare i int = 0;
    begin
        while (i < {CONCURRENT_ATT_CNT}) do
        begin
            insert into test(id) values(:i);
            i = i + 1;
        end
    end
    ^
    set term ;^
    commit;
"""
tmp_isql_cmds = temp_files( [ f'tmp_3323.{i}.sql' for i in range(0,CONCURRENT_ATT_CNT) ] )
tmp_isql_logs = temp_files( [ f'tmp_3323.{i}.log' for i in range(0,CONCURRENT_ATT_CNT) ] )

db = db_factory(init = init_ddl)
act = python_act('db')

@pytest.mark.version('>=3')
def test_1(act: Action, tmp_isql_cmds: List[Path], tmp_isql_logs: List[Path], capsys):

    with act.db.connect() as con, act.db.connect() as con_locker:
        
        con_locker.execute_immediate('update test set id=-id order by id rows 1')

        sql_check_appearance = """
            select s.mon$attachment_id
            from mon$statements s
            where s.mon$attachment_id <> current_connection and s.mon$sql_text containing cast(? as varchar(20))
        """
        with con.cursor() as cur:
            ps, rs = None, None
            try:
                ps = cur.prepare(sql_check_appearance)
                worker_att_list = []
                worker_log_list = []
                worker_pid_list = []

                for worker_i in range(0, CONCURRENT_ATT_CNT):
                    worker_log_list.append( open(tmp_isql_logs[worker_i], 'w') )


                for worker_i in range(0, CONCURRENT_ATT_CNT):
                    if worker_i < CONCURRENT_ATT_CNT-1:
                        id_senior = worker_i+1
                        id_junior = worker_i
                    else:
                        id_senior = 0
                        id_junior = CONCURRENT_ATT_CNT-1

                    sql_worker_dml = f"""
                        set list on;
                        select rdb$get_context('SYSTEM','CLIENT_PID') as client_pid,current_connection from rdb$database;
                        update /* TAG_{worker_i} */ test set id = -id where id in ({id_junior}, {id_senior}) order by id desc rows 2;
                    """

                    f_sql_cmd = open(tmp_isql_cmds[worker_i], 'w')
                    f_sql_cmd.write(sql_worker_dml)
                    f_sql_cmd.close()

                    f_isql_log = worker_log_list[worker_i] # open(tmp_isql_logs[worker_i], 'w')
                    p = subprocess.Popen([act.vars['isql'], '-user', act.db.user, '-password', act.db.password, '-n', '-i', f_sql_cmd.name, act.db.dsn], stdout = f_isql_log, stderr = subprocess.STDOUT)
                    worker_pid_list.append(p)

                    #---------------------------------------------------------------------------
                    # W A I T   F O R   I S Q L    A P P E A R    I N     M O N $    T A B L E S
                    #---------------------------------------------------------------------------
                    t1=py_dt.datetime.now()
                    while True:
                        time.sleep(0.1)
                        t2=py_dt.datetime.now()
                        d1=t2-t1
                        dd = d1.seconds*1000 + d1.microseconds//1000
                        if dd > MAX_WAIT_FOR_ISQL_START_MS:
                            print(f'TIMEOUT EXPIRATION: waiting for ISQL process on iter {worker_i} took {dd} ms which exceeds limit = {MAX_WAIT_FOR_ISQL_START_MS} ms.')
                            break

                        # ::: NB ::: 'ps' returns data, i.e. this is SELECTABLE expression.
                        # We have to store result of cur.execute(<psInstance>) in order to
                        # close it explicitly.
                        # Otherwise AV can occur during Python garbage collection and this
                        # causes pytest to hang on its final point.
                        # Explained by hvlad, email 26.10.24 17:42
                        rs = cur.execute(ps, (f'TAG_{worker_i}',))
                        worker_att = None
                        for r in rs:
                            worker_att = r

                        con.commit()

                        if worker_att:
                            worker_att_list.append(worker_att[0])
                            break

                        # result: attachment_id of just launched ISQL was added to worker_att_list
                        #---------------------------    

                # result: all ISQLs are launched and their attachments are visible in mon$attachments (and can be traversed via worker_att_list)

                kill_sttm = cur.prepare('delete from mon$attachments a where a.mon$attachment_id = ?')

                ###################################################################
                ###   k i l l    a t t a c h m e n t s    o n e - b y - o n e   ###
                ###################################################################
                for worker_id in reversed(worker_att_list):
                    cur.execute(kill_sttm, (worker_id,))

            except DatabaseError as e:
                print( e.__str__() )
                print(e.gds_codes)

            finally:
                if rs:
                    rs.close() # <<< EXPLICITLY CLOSING CURSOR RESULTS
                if ps:
                    ps.free()

                for i,p_isql in enumerate(worker_pid_list):
                    p_isql.wait(MAX_WAIT_FOR_ISQL_FINISH_S)
                    print(f'returncode for ISQL worker #{i}:',p.poll())
                for f in worker_log_list:
                    f.close()



            # All worker logs must contain 'SQLSTATE = 08003' pattern (i.e. 'connection shutdown'):
            p_shutdown = re.compile('SQLSTATE\\s+=\\s+08003', re.IGNORECASE)
            for g in worker_log_list:
                with open(g.name, 'r') as f:
                    txt = ''.join( f.readlines() )
                    if p_shutdown.search(txt):
                        pass
                    else:
                        print('Pattern ',p_shutdown,' NOT FOUND in the log ',g.name,':')
                        print('=== beg of log ===')
                        print(txt)
                        print('=== end of log ===')
            con.commit()

            # NO any ISQL worker must be alive now:
            cur.execute("select a.mon$attachment_id from mon$attachments a where a.mon$system_flag <> 1 and lower(a.mon$remote_process) similar to '%[\\/]isql(.exe)?'")
            for r in cur:
                print('UNEXPECTEDLY remained ISQL attachment:',r[0])

            # All workers had to be completed with retcode = 1:
            expected_stdout = '\n'.join( [ f'returncode for ISQL worker #{i}: 1' for i in range(0, CONCURRENT_ATT_CNT) ])

    act.stdout = capsys.readouterr().out
    act.expected_stdout = expected_stdout
    assert act.clean_stdout == act.clean_expected_stdout

