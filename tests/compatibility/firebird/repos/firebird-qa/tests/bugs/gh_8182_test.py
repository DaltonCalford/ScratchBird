#coding:utf-8

"""
ID:          issue-8182
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8182
TITLE:       IN predicate incorrectly handles single parenthesized subquery as IN-list, instead of table subquery
DESCRIPTION:
NOTES:
    [15.06.2025] pzotov
    Confirmed bug on 6.0.0.835-3da8317; 5.0.3.1661-cfcf0e8
    Checked on 6.0.0.838-0b49fa8; 5.0.3.1666-97178d0; 4.0.6.3213-f015c28; 3.0.13.33813-222a910
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    select sign(count(*))
    from rdb$character_sets
    where rdb$character_set_id in (
        (
            select rdb$character_set_id
            from rdb$collations
        )
    );

"""

act = isql_act('db', test_script, substitutions=[ ('[ \\t]+', ' ') ])

expected_stdout = """
    SIGN 1
"""

@pytest.mark.version('>=3.0.13')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
