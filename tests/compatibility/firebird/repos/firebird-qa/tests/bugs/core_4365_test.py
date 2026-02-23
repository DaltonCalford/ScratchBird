#coding:utf-8

"""
ID:          issue-4687
ISSUE:       4687
TITLE:       Equality predicate distribution does not work for some complex queries
DESCRIPTION:
JIRA:        CORE-4365
FBTEST:      bugs.core_4365
NOTES:
    [29.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set planonly;
    select *
    from (
      select r.rdb$relation_id as id
      from rdb$relations r
        join (
          select g1.rdb$generator_id as id from rdb$generators g1
          union all
          select g2.rdb$generator_id as id from rdb$generators g2
        ) rf on rf.id = r.rdb$relation_id
        left join rdb$procedures p on p.rdb$procedure_id = rf.id
    ) x
    where id = 1;
"""

act = isql_act('db', test_script, substitutions=[('RDB\\$INDEX_[0-9]+', 'RDB$INDEX_*')])

expected_stdout_5x = """
    PLAN JOIN (JOIN (X RF G1 INDEX (RDB$INDEX_*), X RF G2 INDEX (RDB$INDEX_*), X R INDEX (RDB$INDEX_*)), X P INDEX (RDB$INDEX_*))
"""

expected_stdout_6x = """
    PLAN JOIN (JOIN ("X" "RF" "G1" INDEX ("SYSTEM"."RDB$INDEX_*"), "X" "RF" "G2" INDEX ("SYSTEM"."RDB$INDEX_*"), "X" "R" INDEX ("SYSTEM"."RDB$INDEX_*")), "X" "P" INDEX ("SYSTEM"."RDB$INDEX_*"))
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
