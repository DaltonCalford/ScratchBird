#coding:utf-8

"""
ID:          issue-4446
ISSUE:       4446
TITLE:       Expression index may be not used for derived fields or view fields
DESCRIPTION:
JIRA:        CORE-4118
FBTEST:      bugs.core_4118
NOTES:
    [28.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table t (id int, d timestamp);
    create index itd on t computed (cast(d as date));
    commit;

    SET PLAN ON;
    select * from t where cast(d as date) = current_date;
    select * from (select id, cast(d as date) as d from t) where d = current_date;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    PLAN (T INDEX (ITD))
    PLAN (T INDEX (ITD))
"""
expected_stdout_6x = """
    PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."ITD"))
    PLAN ("PUBLIC"."T" INDEX ("PUBLIC"."ITD"))
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
