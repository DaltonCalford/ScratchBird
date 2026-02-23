#coding:utf-8

"""
ID:          issue-5747
ISSUE:       5747
TITLE:       Trace duplicates asci_char(13) in its output (Windows only)
DESCRIPTION:
  We launch trace and create connect to DB with statement line 'select ... from rdb$database'.
  Trace log should contain several lines related to connection, transaction and statement.
  ON WINDOWS these lines should be separated by *pair* of characters: CR + NL.
  Count of these chars should be equal.
  We then open trace log as binary file and read all its content into dict of Counter type,
  thus we can get number of occurences for each character, including CR and NL.
  Finally, we compare number of occurences of CR and NL. Difference has to be no more than 1.
JIRA:        CORE-5477
FBTEST:      bugs.core_5477

NOTES:
  [08.06.2022] pzotov
  Confirmed problem on 3.0.1.32609: trace contains four NL and zero CR characters.
  Adapted for firebird-qa plugin. Checked on 4.0.1.2692, 3.0.8.33535.
"""

import pytest
from firebird.qa import *
from pathlib import Path
from collections import Counter

db = db_factory()

act = python_act('db')
tmp_file = temp_file('tmp_5477_trace.log')

expected_stdout_trace = """
    EXPECTED.
"""

trace = ['time_threshold = 0',
         'log_connections = true',
         'log_transactions = true',
         'log_errors = true',
         'log_statement_finish = true',
         ]

@pytest.mark.trace
@pytest.mark.version('>=3.0.2')
@pytest.mark.platform('Windows')
def test_1(act: Action, tmp_file: Path, capsys):
    with act.trace(db_events=trace, encoding='utf8', encoding_errors='utf8'):
        act.isql(switches=['-q'], charset = 'utf8', input = "select 'Ð' from rdb$database;")
    
    tmp_file.write_bytes( bytearray(''.join(act.trace_log), encoding='utf8', errors = 'ignore') )

    letters_dict={}
    with open( tmp_file,'rb') as f:
        letters_dict = Counter(f.read())

    nl_count = letters_dict[ 10 ]
    cr_count = letters_dict[ 13 ]

    print( 'EXPECTED.' if nl_count >= 1 and abs(nl_count - cr_count) <= 1 else f'FAIL: empty log or NL count differ than CR: nl_count={nl_count}, cr_count={cr_count}' )

    act.expected_stdout = expected_stdout_trace
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
