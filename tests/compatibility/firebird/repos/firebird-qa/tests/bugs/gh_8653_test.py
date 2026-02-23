#coding:utf-8

"""
ID:          n/a
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8653
TITLE:       TRANSACTION_ROLLBACK missing in the trace log when appropriate DB-level trigger fires
DESCRIPTION:
NOTES:
    [20.07.2024] zotov
    ::: ACHTUNG :::
    One need to set 'time_threshold = 0' otherwise trigger_finish can be missed because of too fast execution!
    
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.1042-992bccd; 5.0.3.1683-7bd32d4; 4.0.6.3221; 3.0.13.33813
"""

import re
import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

allowed_patterns = [ r'\)\s+ERROR ', r'(Trigger\s+)?("PUBLIC".)?(")?TRG_TX_ROLLBACK(")?', ]
allowed_patterns = [ re.compile(r, re.IGNORECASE) for r in  allowed_patterns]

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action, capsys):

    init_script = f"""
        set term ^;
        create or alter trigger trg_tx_rollback active on transaction rollback as
        begin
        end
        ^
        set term ;^
        commit;
    """
    act.isql(switches = ['-q', '-nod'], input = init_script, combine_output = True)
    assert act.clean_stdout == ''
    act.reset()

    trace = [ 
              'log_errors = true',
              'time_threshold = 0', # <<<<<<<<<<<<<<   ::: A.C.H.T.U.N.G ::: <<<<<<<<<<<<<<
              'log_trigger_start = true',
              'log_trigger_finish = true',
             ]
    
    with act.trace(db_events = trace, encoding = 'utf8', encoding_errors = 'utf8'):
        with act.db.connect() as con:
            cur = con.cursor()
            cur.execute('select 1 from rdb$database')
            con.rollback()

    for line in act.trace_log:
        if act.match_any(line, allowed_patterns):
            print(line)

    expected_stdout_4x = f"""
        TRG_TX_ROLLBACK (ON TRANSACTION_ROLLBACK)
        TRG_TX_ROLLBACK (ON TRANSACTION_ROLLBACK)
    """

    expected_stdout_5x = f"""
        Trigger TRG_TX_ROLLBACK (ON TRANSACTION_ROLLBACK):
        Trigger TRG_TX_ROLLBACK (ON TRANSACTION_ROLLBACK):
    """

    expected_stdout_6x = f"""
        Trigger "PUBLIC"."TRG_TX_ROLLBACK" (ON TRANSACTION_ROLLBACK):
        Trigger "PUBLIC"."TRG_TX_ROLLBACK" (ON TRANSACTION_ROLLBACK):
    """

    act.expected_stdout = expected_stdout_4x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
