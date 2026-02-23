#coding:utf-8

"""
ID:          issue-6462
ISSUE:       6462
TITLE:       COUNT(DISTINCT <DECFLOAT_FIELD>) leads FB to crash when there are duplicate values of this field
DESCRIPTION:
JIRA:        CORE-6218
FBTEST:      bugs.core_6218
NOTES:
    [03.07.2025] pzotov
    Difference of transactions before and after queries must be checked to be sure that there was no crash.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table test(n decfloat);
    commit;

    insert into test values( 0 );
    insert into test values( 0 );
    commit;

    set term ^;
    execute block as
    begin
        rdb$set_context('USER_TRANSACTION', 'INIT_TX', current_transaction);
    end ^
    set term ;^

    select n as n_grouped_from_test0 from test group by 1; --- [ 1 ]
    select distinct n as n_uniq_from_test0 from test; -- [ 2 ]
    select count(distinct n) as count_uniq_from_test0 from test; -- [ 3 ]
    select current_transaction - cast( rdb$get_context('USER_TRANSACTION', 'INIT_TX') as int) as tx_diff from rdb$database;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    N_GROUPED_FROM_TEST0 0
    N_UNIQ_FROM_TEST0 0
    COUNT_UNIQ_FROM_TEST0 1
    TX_DIFF 0
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):

    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
