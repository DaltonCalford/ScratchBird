#coding:utf-8

"""
ID:          issue-8737
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8737
TITLE:       "Statement 0, <unknown, bug?>" in trace
DESCRIPTION:
NOTES:
    [13.09.2025] pzotov
    Confirmed bug on 6.0.0.1267; 5.0.4.1706.
    Checked on 6.0.0.1273-32a50dc; 5.0.4.1711-4911fbb.
"""
import locale
import pytest
from firebird.qa import *

init_script = """
    recreate table test(x int);
"""

db = db_factory(init = init_script)

INVALID_EXPR = 'select no_such_field from test'
substitutions = [
    ( f'^((?!(SQLSTATE|Column\\s+unknown|{INVALID_EXPR}|unknown(,)?\\s+bug|FAILED\\s+PREPARE_STATEMENT)).)*$', '' ),
    ('.* FAILED\\s+PREPARE_STATEMENT', 'FAILED PREPARE_STATEMENT'),
    ('(-)?Column', 'Column')
]

act = python_act('db', substitutions = substitutions)

trc_events_lst = [
    'time_threshold = 0',
    'log_statement_prepare = true',
]

@pytest.mark.trace
@pytest.mark.version('>=5.0.4')
def test_1(act: Action, capsys):
    with act.trace(db_events = trc_events_lst):
        test_sql = f"""
            set list on;
            connect '{act.db.db_path}' user {act.db.user} password '{act.db.password}';
            {INVALID_EXPR};
        """
        try:
            act.isql(switches = ['-q'], credentials = False, connect_db = False, combine_output = True, input = test_sql, io_enc = locale.getpreferredencoding())
            # act.isql(switches = ['-q'], combine_output = True, input = test_sql, io_enc = locale.getpreferredencoding(), use_db = act.db.db_path, charset = 'utf8')
            print(act.clean_stdout)
            # act.reset()
        except Exception as e:
            print(e)

    for line in act.trace_log:
        print(line)

    act.reset()

    expected_stdout = f"""
        Statement failed, SQLSTATE = 42S22
        Column unknown
        FAILED PREPARE_STATEMENT
        {INVALID_EXPR}
    """
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
