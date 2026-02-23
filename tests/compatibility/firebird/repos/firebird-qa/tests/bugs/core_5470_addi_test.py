#coding:utf-8

"""
ID:          issue-5740-B
ISSUE:       5740
TITLE:       Trace stops any writing to its log if client sends text that can not be transliterated between character sets
DESCRIPTION:
    It was detected that trace can stop any writing to its log if client issues query with character
    that can NOT transliterated between character sets. Such statement and *any* other statements
    that go after will NOT be reflected in the trace if its config contain include_filter with any
    rule (even such trivial as: include_filter = "%").

    Initial discuss: july-2019, subj:  "... fbtrace returned error on call trace_dsql_execute" (mailbox: pz@ibase.ru).
    Letter with example how to reproduce: 16.07.19 22:08.
    Finally this bug was fixed 26.03.2020:
        https://github.com/FirebirdSQL/firebird/commit/70ed61cba88ad70bd868079016cde3b338073db8
    ::: NB :::
    Problem was found  only in FB 3.0; 4.x works OK because of new regexp mechanism in it.

    Test launches trace with include_filter option, and then runs isql with executing three statements.
    First and third statements contrain ascii-only text, second has a character which can not be transliterated from utf8
    to narrow-byte encoding (see 'CHECKED_CHARACTER' and 'CAST_TO_CHARSET' variables).
    We have to check that:
    1. isql actually shows only 1st and 3rd statement, and issues STDERR with SQLSTATE = 22018 for second statement;
    2. trace log contains all *three* statements that were executed by isql, including one that contains <CHECKED_CHARACTER>.
    
    Confirmed bug on 3.0.6.33273: only first statement appears in the trace.
    All fine on 3.0.6.33276: all three statements can be seen in the trace.
JIRA:        CORE-5470
FBTEST:      bugs.core_5470_addi
NOTES:
    [08.06.2022] pzotov
    Adapted for firebird-qa plugin. Confirmed problem on 3.0.6.33273. Checked on 4.0.1.2692, 3.0.8.33535 - all OK.
"""

import pytest
from firebird.qa import *

#CHECKED_CHARACTER = 'Á'
#CHECKED_CHARACTER = 'Ä'
#CHECKED_CHARACTER = 'Ď'
CHECKED_CHARACTER = 'Ð'

CAST_TO_CHARSET = 'win1251'

db = db_factory(utf8filename = True)

substitutions = [('^((?!POINT|point).)*$', '')]
act = python_act('db', substitutions = substitutions)

trace = ['time_threshold = 0',
         'log_initfini = false',
         'log_errors = true',
         'log_statement_finish = true',
         'include_filter = "%(as point)%"',
         ]

cast_sttm = f"select cast('{CHECKED_CHARACTER}' as varchar(20) character set {CAST_TO_CHARSET}) as point from rdb$database"

test_sql = f"""
    set list on;
    select 1 as point from rdb$database;
    {cast_sttm};
    select 3 as point from rdb$database;
"""

expected_stdout_isql = f"""
    POINT                           1
    POINT                           3
"""

expected_stderr_isql = """
    Statement failed, SQLSTATE = 22018
    arithmetic exception, numeric overflow, or string truncation
    -Cannot transliterate character between character sets
"""

expected_stdout_trace = test_sql.replace('set list on;', '').replace(';','')

@pytest.mark.intl
@pytest.mark.trace
@pytest.mark.version('>=3.0.6')
@pytest.mark.platform('Windows')
def test_1(act: Action, capsys):
    
    # 1. Check that ISQL actually shows only 1st and 3rd statement, and issues STDERR with SQLSTATE = 22018:
    act.expected_stdout = expected_stdout_isql
    act.expected_stderr = expected_stderr_isql
    with act.trace(db_events=trace, encoding='utf8', encoding_errors='utf8'):
        act.isql(switches=['-q'], charset = 'utf8', input=test_sql)

    assert (act.clean_stdout == act.clean_expected_stdout and act.clean_stderr == act.clean_expected_stderr)
    act.reset()

    #------------------------------------------------------------

    # 2. Check that trace log contains all three statements that were executed by isql:
    for line in act.trace_log:
        print(line)

    act.expected_stdout = expected_stdout_trace
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
