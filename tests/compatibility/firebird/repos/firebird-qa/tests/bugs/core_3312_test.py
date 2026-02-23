#coding:utf-8

"""
ID:          issue-3679
ISSUE:       3679
TITLE:       Sub-optimal join plan when the slave table depends on the master via the "OR" predicate
DESCRIPTION:
JIRA:        CORE-3312
FBTEST:      bugs.core_3312
NOTES:
    [27.06.2025] pzotov
    Added substitutions in order to suppress digital suffix of indices name (in 'RDB$INDEX_').
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
SET PLANONLY ON;
select *
from rdb$relations r
  join rdb$security_classes sc
    on (r.rdb$security_class = sc.rdb$security_class
      or r.rdb$default_class = sc.rdb$security_class);
select *
from rdb$relations r
  join rdb$security_classes sc
    on (r.rdb$security_class = sc.rdb$security_class and r.rdb$relation_id = 0)
      or (r.rdb$default_class = sc.rdb$security_class and r.rdb$relation_id = 1);
"""


substitutions = [ ('[ \t]+', ' '), ('RDB\\$INDEX_\\d+', 'RDB$INDEX_*') ] 
act = isql_act('db', test_script, substitutions = substitutions)


expected_out_5x = """
    PLAN JOIN (R NATURAL, SC INDEX (RDB$INDEX_*, RDB$INDEX_*))
    PLAN JOIN (R INDEX (RDB$INDEX_*, RDB$INDEX_*), SC INDEX (RDB$INDEX_*, RDB$INDEX_*))
"""

expected_out_6x = """
    PLAN JOIN ("R" NATURAL, "SC" INDEX ("SYSTEM"."RDB$INDEX_*", "SYSTEM"."RDB$INDEX_*"))
    PLAN JOIN ("R" INDEX ("SYSTEM"."RDB$INDEX_*", "SYSTEM"."RDB$INDEX_*"), "SC" INDEX ("SYSTEM"."RDB$INDEX_*", "SYSTEM"."RDB$INDEX_*"))
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_out_5x if act.is_version('<6') else expected_out_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
