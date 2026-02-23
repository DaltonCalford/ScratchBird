#coding:utf-8

"""
ID:          issue-3786
ISSUE:       3786
TITLE:       [FB3] Wrong RDB$PARAMETER_MECHANISM
DESCRIPTION:
JIRA:        CORE-3423
FBTEST:      bugs.core_3423
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter procedure sp_test(IN_ARG INTEGER) returns (OUT_ARG INTEGER) as
    begin
        OUT_ARG=IN_ARG;
    end
    ^
    set term ;^
    commit;

    set list on;
    -- NB: engine treats nulls and zeroes as the same value, so it is enough to check that
    -- parameter mechanism is either null or zero only:
    select
      pp.rdb$parameter_name p_name
     ,iif( coalesce(pp.rdb$parameter_mechanism,0) = 0, 'ZERO_OR_NULL', 'BAD_VALUE' ) p_mechanism
    from rdb$procedure_parameters pp
    where pp.rdb$procedure_name = upper('SP_TEST');
"""

act = isql_act('db', test_script)

expected_stdout = """
    P_NAME                          IN_ARG
    P_MECHANISM                     ZERO_OR_NULL

    P_NAME                          OUT_ARG
    P_MECHANISM                     ZERO_OR_NULL
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

