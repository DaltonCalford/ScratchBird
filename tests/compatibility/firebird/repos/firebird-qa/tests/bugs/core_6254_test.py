#coding:utf-8

"""
ID:          issue-6497
ISSUE:       6497
TITLE:       AV in engine when using SET TRANSACTION and ON TRANSACTION START trigger
  uses EXECUTE STATEMENT against current transaction
DESCRIPTION:
JIRA:        CORE-6254
FBTEST:      bugs.core_6254
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    create or alter trigger trg_tx_start on transaction start as
        declare tx int;
    begin
        execute statement ('select current_transaction from rdb$database') into tx;
    end
    ^
    set term ;^
    commit;

    set transaction;
    select sign(current_transaction) as s from rdb$database;
    commit; -- this raised AV
"""

act = isql_act('db', test_script)

expected_stdout = """
    S                               1
"""

@pytest.mark.version('>=3.0.6')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
