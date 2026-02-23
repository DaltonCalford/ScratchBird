#coding:utf-8

"""
ID:          issue-3543
ISSUE:       3543
TITLE:       exclude_filter doesn't work for <services></section> section of the Trace facility
DESCRIPTION:
JIRA:        CORE-3168
FBTEST:      bugs.core_3168
"""

import pytest
from firebird.qa import *
from firebird.driver import SrvStatFlag

db = db_factory()

act = python_act('db', substitutions=[('^((?!ERROR|ELEMENT|PROPERTIES|STATS|BACKUP|RESTORE).)*$', '')])

expected_stdout = """
   EXCLUDE_FILTER = "DATABASE STATS"
   "DATABASE PROPERTIES"
   "BACKUP DATABASE"
"""

trace = ['log_services = true',
           'exclude_filter = "Database Stats"',
           ]

temp_file = temp_file('test-file')

@pytest.mark.trace
@pytest.mark.version('>=3.0')
def test_1(act: Action, temp_file):
    with act.trace(svc_events=trace), act.connect_server() as srv:
        srv.database.set_sweep_interval(database=act.db.db_path, interval=1234321)
        srv.database.get_statistics(database=act.db.db_path, flags=SrvStatFlag.HDR_PAGES)
        srv.wait()
        srv.database.backup(database=act.db.db_path, backup=temp_file)
        srv.wait()
    act.expected_stdout = expected_stdout
    act.trace_to_stdout(upper=True)
    assert act.clean_stdout == act.clean_expected_stdout
