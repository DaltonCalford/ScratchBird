#coding:utf-8

"""
ID:          525e5665
ISSUE:       https://github.com/FirebirdSQL/firebird/commit/525e5665556f0670483ea032d78b8069347a6490
TITLE:       Corrections after LTT changes
DESCRIPTION:
    Regression in 6.0.0.1403-b1d8d64 (first snapshot with LTT feature): creation of tables/views + trigger ON COMMIT
    (with AUTODDL OFF) may raise "HY000/invalid request BLR at offset 18/-context already in use (BLR error)" at
    final commit.
NOTES:
    [02.02.2026] pzotov
    1. Found in script that is executed at preparing phase of every tests related to statements restart check in RCRC
       (see $QA_HOME/tests/functional/transactions/). Name of script: $QA_HOME/files/read-consist-sttm-restart-DDL.sql
       Workaround (before this fix) was to disable AUTODDL OFF: after this all RCRC-tests started work fine.
       It is reasonable not to revert it (i.e. let AUTODDL remains ON) because this regression has no link to RCRC.
    2. It may look strange but the script presented here is probably MINIMAL for reproducing problem, despite that it
       contains obviously "excessive" DDLs (e.g. it creates tables 'tsrc' and 'flag' that are not used at all).

    Confirmed bug on 6.0.0.1403-b1d8d64 
    Checked on 6.0.0.1403-055ae45.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set autoterm on;
    set autoddl off;
    recreate view v_worker_log as select 1 x from rdb$database;
    recreate view v_test as select 1 x from rdb$database;
    recreate table tlog_want(id int);
    recreate table tlog_done(id int);
    recreate table test(id int);
    commit;

    recreate table test(id smallint);
    recreate table tsrc(id smallint); -- MUST PRESENT, DESPITE THAT NOT USED AT ALL!
    recreate table flag(id smallint); -- MUST PRESENT, DESPITE THAT NOT USED AT ALL!
    recreate view v_test as select * from test;

    recreate table tlog_want(id smallint);
    recreate table tlog_done(id smallint);
    recreate view v_worker_log as select 1 id from rdb$database;
    commit;

    recreate trigger trg_commit inactive on transaction commit position 999 as
    begin
        insert into tlog_want(id) values(1);
    end;
    commit; -- <<< THIS CAUSED BLR ERROR
    select 'Completed' as msg from rdb$database;
"""
substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        MSG Completed
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
