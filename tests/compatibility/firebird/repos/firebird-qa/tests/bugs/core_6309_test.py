#coding:utf-8

"""
ID:          issue-6550
ISSUE:       6550
TITLE:       fbsvcmgr can not decode information buffer returned by gfix list limbo service
DESCRIPTION:
  Test creates two databases and uses fdb.ConnectionGroup() for working with them using two connections.
  Then we add several rows in both databases (using autonomous transactions for one of them) and change state
  of this DB to full shutdown (see 'dbname_a').
  After this we return 'dbname_a' to online.
  At this point this DB must contain dozen transaction in limbo state.
  Both 'gfix -list' and 'fbsvcmgr action_repair rpr_list_limbo_trans' can display only 146 transactions because
  of limited internal buffer size which is used.
  We check that number of lines which are issued by these utilities is equal.
  NB-1: concrete numbers of transactions will DIFFER on SS/SC/CS. We must check only *number* of lines.
  NB-2: we can not use svc.get_limbo_transaction_ids( <dbname> ) because FDB raises exception:
        File "build\\bdist.win-amd64\\egg\\fdb\\services.py", line 736, in get_limbo_transaction_ids
        struct.error: unpack requires a string argument of length 4
      Because of this, external child processes are called to obtain Tx list: gfix and fbcvmgr.

  Confirmed bug on 4.0.0.1633: fbsvcmgr issues 0 rows instead of expected 146.
NOTES:
[21.12.2021] pcisar
  On v3.0.8 & 4.0, the fbsvcmngr reports error: "unavailable database"
  Which makes the test fail
  See also: core_6141_test.py
JIRA:        CORE-6309
FBTEST:      bugs.core_6309
"""

import pytest
import re
from firebird.qa import *
from firebird.driver import DistributedTransactionManager, ShutdownMode, ShutdownMethod

init_script = """
create table test(id int, x int, constraint test_pk primary key(id) using index test_pk) ;
"""

db_a = db_factory(init=init_script, filename='core_6309_a.fdb')
db_b = db_factory(init=init_script, filename='core_6309_b.fdb')

act = python_act('db_a')

LIMBO_COUNT = 250

@pytest.mark.skip("FIXME: see notes")
@pytest.mark.version('>=3.0.7')
def test_1(act: Action, db_b: Database):
    dt_list = []
    with act.db.connect() as con1, db_b.connect() as con2:
        for i in range(LIMBO_COUNT):
            dt = DistributedTransactionManager([con1, con2])
            dt_list.append(dt)
            cur1 = dt.cursor(con1)
            cur1.execute("insert into test (id, x) values (?, ?)", [i, i * 11111])
            cur1.close()
            cur2 = dt.cursor(con2)
            cur2.execute("insert into test (id, x) values (?, ?)", [-i, i * -2222])
            cur2.close()
        for dtc in dt_list:
            # Initiate distributed commit: phase-1
            dtc.prepare()
        # Shut down the first database
        with act.connect_server() as srv:
            srv.database.shutdown(database=act.db.db_path, mode=ShutdownMode.FULL,
                                  method=ShutdownMethod.FORCED, timeout=0)
        #
        while dt_list:
            dtc = dt_list.pop()
            dtc._tra = None # Needed hack to bypass commit and exception
            dtc.close()
        #
        with act.connect_server() as srv:
            srv.database.bring_online(database=act.db.db_path)
    #
    act.gfix(switches=['-list', act.db.dsn])
    gfix_log = act.stdout
    #
    act.reset()
    # [FIXME] Set EXPECTED_STDERR so we can get over "unavailable database" error and fail on assert
    # Remove when svcmgr issue is resolved
    act.expected_stderr = "We expect errors"
    act.svcmgr(switches=['action_repair', 'rpr_list_limbo_trans', 'dbname', act.db.dsn])
    mngr_log = act.stdout
    #print(act.stderr)
    #
    pattern = re.compile('Transaction\\s+(\\d+\\s+(is\\s+)?in\\s+limbo)|(in\\s+limbo(:)?\\s+\\d+)', re.IGNORECASE)
    found_limbos = [0, 0]
    #
    for i, limbo_log in enumerate([gfix_log, mngr_log]):
        for line in limbo_log.splitlines():
            found_limbos[i] += 1 if pattern.search(line) else 0
    # Check [gfix, svsmgr]
    assert found_limbos == [146, 146]


# test_script_1
#---
#
#  import sys
#  import os
#  import shutil
#  import time
#  import datetime
#  import re
#  from datetime import timedelta
#  import subprocess
#  from fdb import services
#
#  LIMBO_COUNT = 250
#
#  os.environ["ISC_USER"] = user_name
#  os.environ["ISC_PASSWORD"] = user_password
#
#  db_name=db_conn.database_name
#  db_conn.close()
#
#  #--------------------------------------------
#
#  def flush_and_close( file_handle ):
#      # https://docs.python.org/2/library/os.html#os.fsync
#      # If you're starting with a Python file object f,
#      # first do f.flush(), and
#      # then do os.fsync(f.fileno()), to ensure that all internal buffers associated with f are written to disk.
#      global os
#
#      file_handle.flush()
#      if file_handle.mode not in ('r', 'rb') and file_handle.name != os.devnull:
#          # otherwise: "OSError: [Errno 9] Bad file descriptor"!
#          os.fsync(file_handle.fileno())
#      file_handle.close()
#
#  #--------------------------------------------
#
#  def cleanup( f_names_list ):
#      global os
#      for f in f_names_list:
#         if type(f) == file:
#            del_name = f.name
#         elif type(f) == str:
#            del_name = f
#         else:
#            print('Unrecognized type of element:', f, ' - can not be treated as file.')
#            del_name = None
#
#         if del_name and os.path.isfile( del_name ):
#             os.remove( del_name )
#
#  #--------------------------------------------
#
#  dbname_a=os.path.join(context['temp_directory'],'c6309_a.fdb')
#  dbname_b=os.path.join(context['temp_directory'],'c6309_b.fdb')
#
#  cleanup( (dbname_a, dbname_b) )
#
#  con1 = fdb.create_database( dsn = 'localhost:' + dbname_a )
#  con2 = fdb.create_database( dsn = 'localhost:' + dbname_b )
#
#  init_ddl = 'create table test(id int, x int, constraint test_pk primary key(id) using index test_pk)'
#  con1.execute_immediate(init_ddl)
#  con1.commit()
#
#  con2.execute_immediate(init_ddl)
#  con2.commit()
#
#  cgr = fdb.ConnectionGroup()
#  cgr.add(con1)
#  cgr.add(con2)
#
#  tx1_list=[]
#  for i in range(0, LIMBO_COUNT):
#     tx1_list += [ con1.trans() ]
#
#  cur_list=[]
#  for i, tx1 in enumerate(tx1_list):
#      tx1.begin()
#      cur=tx1.cursor()
#      cur.execute( "insert into test(id, x) values( ?, ? )", ( i, i*11111 ) )
#      cur.close()
#      tx1.prepare()
#
#  tx2 = con2.trans()
#  cur2=tx2.cursor()
#  cur2.execute( "insert into test(id, x) values( ?, ? )", (-2, -2222222) )
#  cur2.close()
#
#  tx2.prepare()
#  tx2.commit()
#
#  svc = services.connect(host='localhost', user=user_name, password=user_password)
#  svc.shutdown( dbname_a, services.SHUT_FULL, services.SHUT_FORCE, 0)
#
#  for tx1 in tx1_list:
#      try:
#          tx1.close()
#      except:
#          pass
#
#  # Result for DBNAME_A when it will be returned online
#  # and we query table TEST:
#  #     Statement failed, SQLSTATE = HY000
#  #     record from transaction <N> is stuck in limbo
#  # See also "gfix -list <disk>\\path	o\\dbname_a"
#
#  cgr.clear()
#  svc.bring_online( dbname_a, services.SHUT_NORMAL )
#  svc.close()
#  con2.close()
#
#  try:
#      # This will fail for sure because DB state was changed to full shutdown
#      con1.close()
#  except:
#      pass
#
#  svc.close()
#
#
#  f_limbo_gfix=open( os.path.join(context['temp_directory'],'tmp_c6309_gfix_list.log'), 'w', buffering=0)
#  subprocess.call( [ context['gfix_path'], "localhost:" + dbname_a, "-list" ], stdout=f_limbo_gfix, stderr=subprocess.STDOUT )
#  flush_and_close( f_limbo_gfix )
#
#  f_limbo_fbsvc=open( os.path.join(context['temp_directory'],'tmp_c6309_fbsvc_list.log'), 'w', buffering=0)
#  subprocess.call( [ context['fbsvcmgr_path'], "localhost:service_mgr", "action_repair", "rpr_list_limbo_trans", "dbname", dbname_a], stdout=f_limbo_fbsvc, stderr=subprocess.STDOUT )
#  flush_and_close( f_limbo_fbsvc )
#
#  # 'gfix -list' issues:
#  # Transaction 3 is in limbo.
#  # Transaction 5 is in limbo.
#  # ...
#  # (totally 146 lines + "More limbo transactions than fit.  Try again")
#
#  # 'fbsvcmgr rpr_list_limbo_trans' issues:
#  # Transaction in limbo: 3
#  # Transaction in limbo: 5
#  # ...
#  # (totally exactly 146 lines, without any other message).
#
#  p=re.compile( 'Transaction\\s+(\\d+\\s+(is\\s+)?in\\s+limbo)|(in\\s+limbo(:)?\\s+\\d+)', re.IGNORECASE )
#
#  f_list = (f_limbo_gfix, f_limbo_fbsvc)
#  found_limbos = [0, 0]
#  for i, g in enumerate(f_list):
#      with open(g.name, 'r') as f:
#         for line in f:
#             found_limbos[i] += 1 if p.search(line) else 0
#
#  for x in found_limbos:
#      print(x)
#
#  # Cleanup
#  ##########
#  time.sleep(1)
#  cleanup( (f_limbo_gfix, f_limbo_fbsvc, dbname_a, dbname_b) )
#
#
#---
