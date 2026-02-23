#coding:utf-8

"""
ID:          issue-7748
ISSUE:       https://github.com/FirebirdSQL/firebird/pull/7748
TITLE:       Allow collation to be a part of data type
DESCRIPTION:
    Test creates domain based on varchar and table with text columns.
    We check that 'COLLATE' clause can be specified directly after character set
    *and* at the same time DEFAULT (and other) clauses can be specified after COLLATE.
    Before this improvement such specification was not allowed ('token unknown / default' raised)
    Finally, we check that domain info actually present in system tables.
NOTES:
    [25.11.2023] pzotov
    Checked on 6.0.0.150.
    See also: gh_8249_test.py
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set bail on;
    -- following statements must not raise errors:
    create domain dm_test
        varchar(10)
        character set WIN1250
        collate WIN_CZ
        default 'foo'
        not null
        check (value in ('foo', 'rio', 'bar'))
    ;

    create table test(
       f01 varchar(10)
           character set WIN1250
           collate PXW_HUN
           default 'bar'
           not null
           check (f01 in ('foo', 'rio', 'bar'))
      ,f02 dm_test collate PXW_HUN
    );
"""

act = isql_act('db', test_script)

expected_stdout = """
"""

@pytest.mark.version('>=6.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
