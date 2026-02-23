#coding:utf-8

"""
ID:          trigger.database.trigger_on_disconnect_06
TITLE:       ON DISCONNECT trigger: print exceptions (including cancelling) to trace if log_errors = true
DESCRIPTION: 
    Improvement was introduced 06-apr-2023, commit:
    https://github.com/FirebirdSQL/firebird/commit/ddec610a08fa3f1a6762d693f9381745a4ecaf84
    Test creates table 'detach_audit' which has UNIQUE constraint and is fulfilled from ON DISCONNECT trigger
    if current user not SYSDBA. Code in that trigger attempts to violate such constraint and appropriate
    exception will raise during its execurion.

    We create non-privileged user ('tmp_worker') and make connection to database. This user makes attach
    and immediately closes connection after this. Execution of trigger will fail but exception will be
    suppressed (according to the documentation: "uncaught exceptions rollback the transaction, disconnect
    the attachment and are swallowed").

    Trace that is launched before this connection must contain messages about errors during statement execution,
    cleanup attachment and violation of PK/UK (with specifying problematic key and line/column in PSQL code).

    Finally, we check that table 'detach_audit' has ONE record (containing name of non-priv user).
NOTES:
    [28.02.2023] pzotov
    Checked on 5.0.0.964 SS/CS.
"""

import pytest
from firebird.qa import *
from firebird.driver import ShutdownMode, ShutdownMethod, DatabaseError
import locale
import re
import time

tmp_worker = user_factory('db', name='tmp_worker', password='123')
db = db_factory()

substitutions = [ ('.* FAILED EXECUTE_TRIGGER_FINISH', 'FAILED EXECUTE_TRIGGER_FINISH'),
                  ('.* ERROR AT JStatement::execute', 'ERROR AT JStatement::execute'),
                  ('.* ERROR AT purge_attachment', 'ERROR AT purge_attachment'),
                  ('violation of PRIMARY or UNIQUE KEY constraint .*', 'violation of PRIMARY or UNIQUE KEY constraint'),
                  ('Problematic key value is .*', 'Problematic key value is'),
                  ('.* line: \\d+, col: \\d+', '')
                ]

act = python_act('db', substitutions = substitutions)

@pytest.mark.trace
@pytest.mark.version('>=5.0')
def test_1(act: Action, tmp_worker: User, capsys):

    init_sql  = f"""
        recreate table detach_audit(
            id int generated always as identity
            ,dts timestamp default 'now'
            ,who varchar(31) default current_user
            ,memo varchar(10)
            ,constraint detach_audit_unq unique(memo)
        );
        commit;
        grant select, insert on detach_audit to public;

        set term ^;
        create trigger trg_disconnect on disconnect as
        begin
           if ( current_user != '{act.db.user}' ) then
           begin
                execute statement ('insert into detach_audit(memo) values(''point-1'')')
                with autonomous transaction
                ;
                execute statement ('insert into detach_audit(memo) values(''point-1'')')
                with autonomous transaction
                ;
           end
        end
        ^
        set term ;^
        commit;
    """

    act.expected_stdout = ''
    act.isql(switches=['-q'], input = init_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    trace_cfg_items = [
        'log_connections = true',
        'log_transactions = true',
        'time_threshold = 0',
        'log_errors = true',
        'log_statement_finish = true',
        'log_trigger_finish = true',
        'max_sql_length = 32768',
    ]

    # ---------------------------------------------------

    with act.trace(db_events = trace_cfg_items, encoding=locale.getpreferredencoding()):
        with act.db.connect(user = tmp_worker.name, password = tmp_worker.password) as con_worker:
            pass

    allowed_patterns = \
    (
         re.escape(') ERROR AT JStatement::execute')
        ,re.escape(') ERROR AT purge_attachment')
        ,'335544665 : violation of PRIMARY or UNIQUE KEY constraint'
        ,'335545072 : Problematic key value is'
        ,"335544842 : At trigger 'TRG_DISCONNECT' line: \\d+, col: \\d+"
    )
    allowed_patterns = [ re.compile(p, re.IGNORECASE) for p in allowed_patterns ]

    for line in act.trace_log:
        if line.strip():
            if act.match_any(line.strip(), allowed_patterns):
                print(line.strip())

    expected_trace_log = """
        2023-02-28T20:32:41.8660 (28180:000000000C450040) ERROR AT JStatement::execute
        335544665 : violation of PRIMARY or UNIQUE KEY constraint "DETACH_AUDIT_UNQ" on table "DETACH_AUDIT"
        335545072 : Problematic key value is ("MEMO" = 'point-1')

        2023-02-28T20:32:41.8660 (28180:000000000C450040) ERROR AT purge_attachment
        335544665 : violation of PRIMARY or UNIQUE KEY constraint "DETACH_AUDIT_UNQ" on table "DETACH_AUDIT"
        335545072 : Problematic key value is ("MEMO" = 'point-1')
        335544842 : At trigger 'TRG_DISCONNECT' line: 8, col: 17
    """

    act.expected_stdout = expected_trace_log
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()

    # ---------------------------------------------------

    expected_final_chk = """
        ID                              1
        WHO                             TMP_WORKER
        Records affected: 1
    """

    act.expected_stdout = expected_final_chk
    act.isql(switches=['-q'], input = 'set count on;set list on;select id,who from detach_audit;', combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
    act.reset()
