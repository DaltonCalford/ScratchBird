#coding:utf-8

"""
ID:          issue-6171
ISSUE:       6171
TITLE:       Add context variables with compression and encryption status of current connection
DESCRIPTION:
JIRA:        CORE-5913
FBTEST:      bugs.core_5913
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    select
         iif( rdb$get_context('SYSTEM','WIRE_COMPRESSED') is not null, 'DEFINED', '<NULL>') as ctx_wire_compressed
        ,iif( rdb$get_context('SYSTEM','WIRE_ENCRYPTED') is not null, 'DEFINED', '<NULL>') as ctx_wire_encrypted
    from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    CTX_WIRE_COMPRESSED             DEFINED
    CTX_WIRE_ENCRYPTED              DEFINED
"""

@pytest.mark.version('>=3.0.4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
