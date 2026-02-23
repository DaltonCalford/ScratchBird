#coding:utf-8

"""
ID:          issue-6705
ISSUE:       6705
TITLE:       Report replica mode through isc_database_info, MON$DATABASE and SYSTEM context
DESCRIPTION:
  Text verifies only ability to query replication-related info from mon$database and system context namespace.
  Query to isc_database_info does not perform.
JIRA:        CORE-6474
FBTEST:      bugs.core_6474
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select mon$replica_mode as mon_replica_mode from mon$database;
    select '>' || rdb$get_context('SYSTEM','REPLICA_MODE') || '<' as ctx_replica_mode from rdb$database;
"""

act = isql_act('db', test_script, substitutions=[('[ \t]+', ' ')])

expected_stdout = """
    MON_REPLICA_MODE                0
    CTX_REPLICA_MODE                <null>
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
