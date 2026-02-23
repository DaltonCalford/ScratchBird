#coding:utf-8

"""
ID:          issue-2586
ISSUE:       2586
TITLE:       Join of SP with view or table may fail with "No current record for fetch operation"
DESCRIPTION:
JIRA:        CORE-2155
FBTEST:      bugs.core_2155
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set term ^;
    create or alter procedure sp_test(a_id int) returns (a_dup int) as
    begin
        a_dup = 2*a_id;
        suspend;
    end
    ^
    set term ;^

    create or alter view v_relations_a as
    select rdb$relation_id, rdb$field_id
    from rdb$relations;

    create or alter view v_relations_b as
    select dummy_alias.rdb$relation_id, dummy_alias.rdb$field_id
    from rdb$relations as dummy_alias;
    commit;

    set planonly;

    select v.rdb$relation_id, p.*
    from v_relations_a v
    INNER join sp_test(v.rdb$field_id) p on 1=1;

    select v.rdb$relation_id, p.*
    from v_relations_b v
    INNER join sp_test(v.rdb$field_id) p on 1=1;
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    PLAN JOIN (V RDB$RELATIONS NATURAL, P NATURAL)
    PLAN JOIN (V DUMMY_ALIAS NATURAL, P NATURAL)
"""

expected_stdout_6x = """
    PLAN JOIN ("V" "SYSTEM"."RDB$RELATIONS" NATURAL, "P" NATURAL)
    PLAN JOIN ("V" "DUMMY_ALIAS" NATURAL, "P" NATURAL)
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
