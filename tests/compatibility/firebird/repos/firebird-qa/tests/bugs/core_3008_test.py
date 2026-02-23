#coding:utf-8

"""
ID:          issue-3389
ISSUE:       3389
TITLE:       Add attachment's CHARACTER SET name into corresponding trace records
DESCRIPTION:
JIRA:        CORE-3008
FBTEST:      bugs.core_3008
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('^((?!ERROR|ELEMENT|SYSDBA:NONE).)*$', ''),
                                        ('.*SYSDBA:NONE', 'SYSDBA:NONE'), ('TCPV.*', 'TCP')])

expected_stdout = """
    SYSDBA:NONE, UTF8, TCP
    SYSDBA:NONE, UTF8, TCP
    SYSDBA:NONE, ISO88591, TCP
    SYSDBA:NONE, ISO88591, TCP
"""

trace = ['log_connections = true',
           'time_threshold = 0',
           ]

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    with act.trace(db_events=trace):
        with act.db.connect(charset='utf8'):
            pass
        with act.db.connect(charset='iso8859_1'):
            pass
    act.expected_stdout = expected_stdout
    act.trace_to_stdout(upper=True)
    assert act.clean_stdout == act.clean_expected_stdout
