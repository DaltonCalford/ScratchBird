#coding:utf-8

"""
ID:          issue-5058
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/5058
TITLE:       Bugcheck 167 (invalid SEND request) while working with GTT from several attachments (using EXECUTE STATEMENT ... ON EXTERNAL and different roles)
DESCRIPTION:
JIRA:        CORE-4754
FBTEST:      bugs.core_4754
NOTES:
    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *
from firebird.driver import tpb, Isolation, TraAccessMode, DatabaseError

init_script = """
    recreate global temporary table gtt_session(x int, y int) on commit preserve rows;
    commit;
"""


db = db_factory(init=init_script)

act = python_act('db')

expected_stdout_5x = """
    Error-1:
    lock conflict on no wait transaction
    -unsuccessful metadata update
    -object TABLE "GTT_SESSION" is in use
"""

expected_stdout_6x = """
    Error-1:
    lock conflict on no wait transaction
    -unsuccessful metadata update
    -object TABLE "PUBLIC"."GTT_SESSION" is in use
"""

@pytest.mark.version('>=3')
def test_1(act: Action, capsys):
    custom_tpb = tpb(isolation=Isolation.READ_COMMITTED_RECORD_VERSION,
                     access_mode=TraAccessMode.WRITE, lock_timeout=0)
    with act.db.connect() as con1:
        tx1a = con1.transaction_manager(custom_tpb)
        cur1a = tx1a.cursor()
        tx1b = con1.transaction_manager(custom_tpb)
        cur1b = tx1b.cursor()
        try:
            cur1a.execute("insert into gtt_session select rand()*10, rand()*10 from rdb$types")
            cur1b.execute("create index gtt_session_x_y on gtt_session computed by (x+y)")
            tx1b.commit() # WI-V2.5.6.27013 issues here: lock conflict on no wait transaction unsuccessful metadata update object TABLE "GTT_SESSION" is in use -901 335544345
            tx1a.commit()
        except DatabaseError as e:
            print('Error-1:')
            msg = e.args[0]
            print(msg)
    #
    if not msg.split():
        # 2.5.5: control should NOT pass here at all!
        with act.db.connect() as con2:
            try:
                tx2a = con2.transaction_manager()
                cur2a = tx2a.cursor()
                cur2a.execute("insert into gtt_session select rand()*11, rand()*11 from rdb$types")
            except DatabaseError as e:
                print('Error-2:')
                print(e.args[0])
    #
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
