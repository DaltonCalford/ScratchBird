#coding:utf-8

"""
ID:          issue-6391
ISSUE:       6391
TITLE:       Error "connection lost to database" could happen when application creates few local attachments (using XNET) simultaneously
DESCRIPTION:
    Test uses 15 threads and each of them launches loop for 10 iterations with making attach/detach from DB.
    Each instance of worker thread has dict() for storing its ID (as a key) and pair of values (success and fail count) as value.

    We have to ensure at the final point that for every of <THREADS_CNT> threads:
    * 1) number of SUCCESSFUL attempts is equal to limit that is declared here as LOOP_CNT.
    * 2) number of FAILED attempts is ZERO.

    Confirmed bug on 4.0.0.1598, 3.0.5.33166 (checked both SS and CS).
    It was enough 3 threads (which tried to establish attachments at the same time) to get runtime error:
    "- SQLCODE: -901 / - connection lost to database".

    Checked on 4.0.0.1607, 3.0.5.33171.
JIRA:        CORE-6142
FBTEST:      bugs.core_6142
NOTES:
    [20.08.2022] pzotov
        Confirmed again problem with 4.0.0.1598, 3.0.5.33166.
        Checked on 5.0.0.591, 4.0.1.2692, 3.0.8.33535

    [02.06.2025] pzotov
        Re-implemented in order to see details about failed attachments (accumulate them in separate logs and print at final of test).
        Checked on 6.0.0.795

        ::: NB :::
        Special setting DEBUG_KIND_OF_INVALID_DATA presents in this test in order to force connection to be failed and check output.
        When this setting is 'invalid_pass' then weird message after test summary:
        ==========
        Exception ignored in atexit callback: <function _api_shutdown at 0x000001618D4240E0>
        Traceback (most recent call last):
          File "C:/Python3x/Lib/site-packages/firebird/driver/core.py", line 161, in _api_shutdown
            provider.shutdown(0, -3) # fb_shutrsn_app_stopped
            ^^^^^^^^^^^^^^^^^^^^^^^^
          File "C:/Python3x/Lib/site-packages/firebird/driver/interfaces.py", line 1315, in shutdown
            self._check()
          File "C:/Python3x/Lib/site-packages/firebird/driver/interfaces.py", line 113, in _check
            raise self.__report(DatabaseError, self.status.get_errors())
        firebird.driver.types.DatabaseError: connection shutdown
        ==========
        Problem exists on 4.x ... 6.x (SS/CS).
        No such message if we set DEBUG_KIND_OF_INVALID_DATA = 'not_valid_db' and try to make connection to some file that for is not valid .fdb

        The reason currently remains unknown.
"""

import os
import threading
import datetime as py_dt
from typing import List
from pathlib import Path
import time

import pytest
from firebird.qa import *
from firebird.driver import connect, driver_config, NetProtocol, DatabaseError

###########################
###    S E T T I N G S  ###
###########################

# Number of threads to start:
THREADS_CNT = 15

# Number of iterations to make connect /  disconnect for every started thread:
LOOP_CNT = 10

##############################
DEBUG_KIND_OF_INVALID_DATA = '' # 'not_valid_db' / 'invalid_pass'
#DEBUG_KIND_OF_INVALID_DATA = 'not_valid_db'
#DEBUG_KIND_OF_INVALID_DATA = 'invalid_pass'
##############################

tmp_logs = temp_files( [ f'tmp_6142.{i}.log' for i in range(THREADS_CNT) ] )

tmp_user = user_factory('db', name='tmp$core_6142', password='123', plugin = 'Srp')
db = db_factory()

act = python_act('db')

#---------------------
def showtime():
     return ''.join( (py_dt.datetime.now().strftime("%H:%M:%S.%f")[:11],'. ') )

#---------------------

def make_db_attach(thread_object):

   mon_sql = f"select count(*) from mon$attachments where mon$user = '{thread_object.tmp_user.name.upper()}'"

   with open(thread_object.tmp_log,'a') as f_thread_log:

       for iter in range(thread_object.num_of_iterations):
           msg_prefix = f"Thread {thread_object.thr_idx}, iter {iter}/{thread_object.num_of_iterations-1}"
           f_thread_log.write( showtime() + f"{msg_prefix} - trying to make connection\n" )

           db_bak = thread_object.db_cfg_object.database.value # need for DEBUG_KIND_OF_INVALID_DATA == 'not_valid_db'
           try:
               a_pass = thread_object.tmp_user.password
               if DEBUG_KIND_OF_INVALID_DATA and thread_object.thr_idx == 2 and iter % 3 == 0:
                   if DEBUG_KIND_OF_INVALID_DATA == 'not_valid_db':
                       thread_object.db_cfg_object.database.value = str(thread_object.tmp_act.vars['bin-dir'] / 'fbclient.dll')
                   elif DEBUG_KIND_OF_INVALID_DATA == 'invalid_pass':
                       a_pass = 't0ta11y@wrong'
                   else:
                       pass

               with connect( thread_object.db_cfg_object.name, user = thread_object.tmp_user.name, password = a_pass ) as con:
                   f_thread_log.write( showtime() + f"{msg_prefix} - established, {con.info.id=}\n" )

                   # Accumulate counter of SUCCESSFULY established attachments:
                   thread_object.pass_lst.append(iter)

           except DatabaseError as e:
               # Accumulate counter of FAILED attachments:
               thread_object.fail_lst.append(iter)
               f_thread_log.write(f'### EXCEPTION ###\n')
               f_thread_log.write(e.__str__() + '\n')
               for x in e.gds_codes:
                   f_thread_log.write(str(x) + '\n')
           finally:
               thread_object.db_cfg_object.database.value =  db_bak

#---------------------

class workerThread(threading.Thread):
   def __init__(self, act: Action, db_cfg_object, thr_idx, threads_cnt, num_of_iterations, tmp_user, tmp_logs):
       threading.Thread.__init__(self)
       self.db_cfg_object = db_cfg_object
       self.thr_idx = thr_idx
       self.threads_cnt = threads_cnt
       self.num_of_iterations = num_of_iterations
       self.tmp_user = tmp_user
       self.tmp_log = tmp_logs[thr_idx]

       self.tmp_act = act
       self.pass_lst = []
       self.fail_lst = []

   def run(self):
       with open(self.tmp_log, 'w') as f_thread_log:
           f_thread_log.write( showtime() + f"Starting thread {self.thr_idx} / {self.threads_cnt-1}\n" )
       
       make_db_attach(self)

       with open(self.tmp_log, 'a') as f_thread_log:
           f_thread_log.write( showtime() + f"Exiting thread {self.thr_idx} / {self.threads_cnt-1}\n" )

#---------------------

@pytest.mark.version('>=3.0.5')
@pytest.mark.platform('Windows')
def test_1(act: Action, tmp_user: User, tmp_logs: List[Path], capsys):

    srv_config = driver_config.register_server(name = 'test_srv_core_6142', config = '')

    # Create new threads:
    # ###################
    threads_list=[]
    for thr_idx in range(THREADS_CNT):
        
        db_cfg_object = driver_config.register_database(name = f'test_db_core_6142_{thr_idx}')
        db_cfg_object.database.value = str(act.db.db_path)
        db_cfg_object.server.value = 'test_srv_core_6142'
        db_cfg_object.protocol.value = NetProtocol.XNET

        threads_list.append( workerThread( act, db_cfg_object, thr_idx, THREADS_CNT, LOOP_CNT, tmp_user, tmp_logs ) )

    # Start new Threads
    # #################
    for t in threads_list:
        t.start()

    # Wait for all threads to complete
    for t in threads_list:
        t.join()

    # not helps -- time.sleep(151)

    if set([len(t.pass_lst) for t in threads_list]) == set((LOOP_CNT,)):
        # All threads could establish connections using XNET on <LOOP_CNT> iterations.
        print('Expected.')
    else:
        for t in threads_list:
            if t.fail_lst:
                print(f'Thread {t.thr_idx} - failed attempts to make connection on iterations:')
                print(t.fail_lst)
                print('Check log:')
                with open(t.tmp_log, 'r') as f:
                    print(f.read())
                print('*' * 50)

    act.expected_stdout = """
        Expected.
    """
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
