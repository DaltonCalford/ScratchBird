#coding:utf-8

"""
ID:          issue-2630
ISSUE:       2630
TITLE:       RDB$VIEW_RELATIONS is not cleaned when altering a view
DESCRIPTION:
JIRA:        CORE-2202
FBTEST:      bugs.core_2202
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table table_1 (id integer);
    recreate table table_2 (id integer);
    recreate table table_3 (id integer);

    create or alter view vw_table(id) as
    select id from table_1;
    commit;

    create or alter view vw_table(id) as
    select id from table_2;
    commit;

    create or alter view vw_table(id) as
    select id
    from table_3;
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    set width vew_name 31;
    set width rel_name 31;
    set width ctx_name 31;
    select
         rdb$view_name as vew_name
        ,rdb$relation_name as rel_name
        ,rdb$view_context
        ,rdb$context_name as ctx_name
    from rdb$view_relations rv;
"""

substitutions = [ ('[ \t]+', ' ') ]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    VEW_NAME VW_TABLE
    REL_NAME TABLE_3
    RDB$VIEW_CONTEXT 1
    CTX_NAME TABLE_3
"""

expected_stdout_6x = """
    VEW_NAME VW_TABLE
    REL_NAME TABLE_3
    RDB$VIEW_CONTEXT 1
    CTX_NAME "PUBLIC"."TABLE_3"
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
