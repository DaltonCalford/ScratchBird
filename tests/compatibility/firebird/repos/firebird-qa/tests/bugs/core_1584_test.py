#coding:utf-8

"""
ID:          issue-2004
ISSUE:       2004
TITLE:       Server crashed or bugcheck when inserting in monitoring tables.
DESCRIPTION:
JIRA:        CORE-1584
FBTEST:      bugs.core_1584
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    insert into mon$statements(
        mon$statement_id
        ,mon$attachment_id
        ,mon$transaction_id
        ,mon$state
        ,mon$timestamp
        ,mon$sql_text
        ,mon$stat_id
    ) values(
        1
        ,current_connection
        ,current_transaction
        ,1
        ,'now'
        ,null
        ,1
    );
    set list on;
    select 1 as x from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    X                               1
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    operation not supported
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

