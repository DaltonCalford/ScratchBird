#coding:utf-8

"""
ID:          issue-7183
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/7183
TITLE:       Regression when derived table has column evaluated as result of subquery with IN(), ANY() or ALL() predicate: "invalid BLR at offset ... / context already in use"
NOTES:
    [27.02.2023] pzotov
    Confirmed bug on 5.0.0.494.
    Checked on 5.0.0.959 - all OK.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;

    -----------------------   1   -----------------------
    select B from (
        select 
            iif( exists(select 1 from rdb$database), 1, 0) b
        from rdb$database
    )
    where B > 0
    ;
     
    -----------------------   2   -----------------------
    select B from (
        select 
            iif( 2 in (select 2 from rdb$database), 2, 0) B
        from rdb$database
    )
    where B > 0
    ;
     
    -----------------------   3   -----------------------
    select B from (
        select 
            iif( 3 = any (select 3 from rdb$database), 3, 0) B
        from rdb$database
    )
    where B > 0
    ;
     
    -----------------------   4   -----------------------
    select B from (
        select 
            iif( 4 = all (select 4 from rdb$database), 4, 0) B
        from rdb$database
    )
    where B > 0
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    1
    2
    3
    4
"""

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
