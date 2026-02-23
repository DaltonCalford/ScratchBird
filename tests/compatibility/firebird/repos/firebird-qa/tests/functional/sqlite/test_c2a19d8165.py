#coding:utf-8

"""
ID:          c2a19d8165
ISSUE:       https://www.sqlite.org/src/tktview/c2a19d8165
TITLE:       Incorrect LEFT JOIN when FROM clause contains nested subqueries
DESCRIPTION:
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    select a.*, b.*, c.*
    from (
        select 'apple' as fruit_a from rdb$database
        union all
        select 'banana' from rdb$database
    ) a
    join (
        select 'apple' as fruit_b from rdb$database
        union all 
        select 'banana' from rdb$database
    ) b on a.fruit_a = b.fruit_b
    left join (
        select 1 as isyellow from rdb$database
    ) c on b.fruit_b = 'banana';
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    FRUIT_A apple
    FRUIT_B apple
    ISYELLOW <null>

    FRUIT_A banana
    FRUIT_B banana
    ISYELLOW 1
    
    Records affected: 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
