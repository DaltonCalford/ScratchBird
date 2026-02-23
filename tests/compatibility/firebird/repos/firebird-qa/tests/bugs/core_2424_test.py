#coding:utf-8

"""
ID:          issue-2840
ISSUE:       2840
TITLE:       Make CREATE VIEW infer column names for views involving a GROUP BY clause or derived table
DESCRIPTION:
JIRA:        CORE-2424
FBTEST:      bugs.core_2424
NOTES:
    [26.06.2025] pzotov
    Reimplemented. It is enough just to try to run command CREATE VIEW with check presence 
    of appropriate record in RDB$RELATIONS.
    No need in 'SHOW VIEW'.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create view v_test as
    select d.rdb$relation_id
    from rdb$database d
    group by d.rdb$relation_id;
    commit;
    select count(*) as view_created from rdb$relations where rdb$relation_name = upper('v_test');
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    VIEW_CREATED 1
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
