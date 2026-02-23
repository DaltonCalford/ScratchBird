#coding:utf-8

"""
ID:          issue-6390
ISSUE:       6390
TITLE:       fbsvcmgr action_repair rpr_list_limbo_trans does not show list of transactions in LIMBO state
DESCRIPTION:
  Test creates two databases with the same DDL (single table with single field): DBNAME_A, DBNAME_B.
  Then it makes instance of fdb.ConnectionGroup() for adding to it two connections (start distibuted work).
  First connection adds bulk of records, each in separate transaction. Second connection adds only one record.
  Number of separate transactions which are used for inserting records see in variable LIMBO_COUNT,
  and it must be not less then 150 (at least for the moment when this test is written: dec-2019).
  Then we change state of DBNAME_A to full shutdown, without doing commit or retain before this.
  Finally, we return this database state to online.
  Since that point header of DBNAME_A contains some data about limbo transactions.
  We make output of them using two ways: gfix -list and fbsvcmgr rpr_list_limbo_trans.
  Output should contain lines with ID of transactions in limbo state.
  NOTE: NOT ALL TRANSACTIONS CAN BE SHOWN BECAUSE THEIR LIST CAN BE EXTREMELY LONG.
  We count number of lines with limbo info using regexp and check that number of these lines equal to expected,
  ignoring concrete values of transaction IDs.

  NB-1.
  Output from gfix and fbsvcmgr differs, see pattern_for_limbo_in_gfix_output and pattern_for_limbo_in_fsvc_output.

  NB-2.
  Only 'gfix -list' produces output which final row is: "More limbo transactions than fit. Try again".
  No such message in the output of fbsvcmgr, it just show some Tx ID (last that it can display).
NOTES:
[21.12.2021] pcisar
  On v3.0.8 & 4.0, the fbsvcmngr reports error: "unavailable database"
  Which makes the test fail
  gfix works fine, although the outpout is more verbose than original test expected
  See also: core_6309_test.py
JIRA:        CORE-6141
FBTEST:      bugs.core_6141
"""

import pytest
import re
from firebird.qa import *
from firebird.driver import tpb, Isolation, DistributedTransactionManager, ShutdownMode, \
     ShutdownMethod

init_script = """
create table test(id int, x int, constraint test_pk primary key(id) using index test_pk) ;
"""

db_a = db_factory(sql_dialect=3, init=init_script, filename='core_6141_a.fdb')
db_b = db_factory(sql_dialect=3, init=init_script, filename='core_6141_b.fdb')

act = python_act('db_a', substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    Number of lines related to limbo Tx in 'gfix -list' output: 146
    Number of lines related to limbo Tx in 'fbsvcmgr rpr_list_limbo_trans' output: 146
"""

LIMBO_COUNT = 255

@pytest.mark.skip("FIXME: see notes")
@pytest.mark.version('>=3.0')
def test_1(act: Action, db_b: Database, capsys):
    dt_list = []
    custom_tpb = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION, lock_timeout=0)
    with act.db.connect() as con1, db_b.connect() as con2:
        for i in range(LIMBO_COUNT):
            dt = DistributedTransactionManager([con1, con2], custom_tpb)
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
    # Set EXPECTED_STDERR so we can get over "unavailable database" error and fail on assert
    # Remove when svcmgr issue is resolved
    act.expected_stderr = "We expect errors"
    act.svcmgr(switches=['action_repair', 'rpr_list_limbo_trans', 'dbname', act.db.dsn])
    mngr_log = act.stdout
    # Show error returned, remove when svcmgr issue is resolved
    print(act.stderr)
    #
    pattern_for_gfix_output = re.compile('Transaction\\s+\\d+\\s+.*limbo', re.IGNORECASE)
    pattern_for_fsvc_output = re.compile('Transaction\\s+in\\s+limbo:\\s+\\d+', re.IGNORECASE)
    #
    for log_name, limbo_log, pattern in [('gfix -list', gfix_log, pattern_for_gfix_output),
                                 ('fbsvcmgr rpr_list_limbo_trans', mngr_log, pattern_for_fsvc_output)]:
        lines_with_limbo_info = 0
        msg = f"Number of lines related to limbo Tx in '{log_name}' output: "
        for line in limbo_log.splitlines():
            if pattern.search(line):
                lines_with_limbo_info += 1
            #else:
                #print(f'Additional output from {log_name}: {line}')
        print(msg + str(lines_with_limbo_info))
    # Check
    act.reset()
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
