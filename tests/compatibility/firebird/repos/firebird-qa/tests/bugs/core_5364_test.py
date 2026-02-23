#coding:utf-8

"""
ID:          issue-5637
ISSUE:       5637
TITLE:       gfix -online normal <db> (being issued in window #1) does not produce error
  when there is sysdba attachment in window #2
DESCRIPTION:
  We create new DB and immediately change its state to single-user maintanance.
  Then we attach to this DB ans run (in separate process) 'gfix -online normal <localhost:this_db>'.
  This command must produce in its STDERR error: "database ... shutdown" - and we check that this actually occurs.
  Also, we check that after reconnect to this DB value of mon$database.mon$shutdown_mode remains the same: 2.
JIRA:        CORE-5364
FBTEST:      bugs.core_5364
"""

import pytest
from firebird.qa import *
from firebird.driver import ShutdownMode, ShutdownMethod

db = db_factory()

act = python_act('db', substitutions=[('database .* shutdown', 'database shutdown')])

expected_stderr = """
    database /test/test.fdb shutdown
"""

@pytest.mark.version('>=3.0.6')
def test_1(act: Action):
    with act.connect_server() as srv:
        srv.database.shutdown(database=act.db.db_path, mode=ShutdownMode.SINGLE,
                              method=ShutdownMethod.FORCED, timeout=0)
        with act.db.connect() as con:
            c = con.cursor()
            sh_mode = c.execute('select mon$shutdown_mode from mon$database').fetchone()[0]
            act.expected_stderr = expected_stderr
            act.gfix(switches=['-online', 'normal', act.db.dsn])
    assert sh_mode == 2
    assert act.clean_stderr == act.clean_expected_stderr
