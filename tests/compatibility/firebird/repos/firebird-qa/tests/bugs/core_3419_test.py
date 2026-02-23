#coding:utf-8

"""
ID:          issue-3782
ISSUE:       3782
TITLE:       Recurse leads to hangs/crash server
DESCRIPTION:
JIRA:        CORE-3419
FBTEST:      bugs.core_3419
NOTES:
    [27.06.2025] pzotov
    Reimplemented: it is enought to check that first lines of error message appear, w/o name of trigger.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import locale
import pytest
from firebird.qa import *

db = db_factory()
test_script = """
    set autoddl off;
    commit;
    recreate table test(id int);
    commit;
    set term ^;
    -- This trigger will fire 1001 times before exception raising:
    create or alter trigger tx_trg
    active on transaction start position 0
    as
    begin
        in autonomous transaction do
        insert into test(id) values(1);
    end
    ^
    set term ;^
    commit;
    set transaction;
"""

act = isql_act('db', substitutions=[('(-)?At trigger .*', '')])

expected_out = f"""
    Statement failed, SQLSTATE = 54001
    Too many concurrent executions of the same request
"""

@pytest.mark.version('>=3.0')
def test_2(act: Action):
    act.expected_stdout = expected_out
    act.isql(switches=['-q'], input = test_script, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
