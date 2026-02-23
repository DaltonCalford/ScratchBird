#coding:utf-8

"""
ID:          issue-4476
ISSUE:       4476
TITLE:       New permission types are not displayed by ISQL
DESCRIPTION:
JIRA:        CORE-4149
FBTEST:      bugs.core_4149
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
    recreate table test(id int);
    commit;
    grant select on test to public;
    commit;
    show grants;

    create sequence g_test;
    commit;

    grant usage on sequence g_test to public;
    commit;
    show grants;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    /* Grant permissions for this database */
    GRANT SELECT ON TEST TO PUBLIC

    /* Grant permissions for this database */
    GRANT SELECT ON TEST TO PUBLIC
    GRANT USAGE ON SEQUENCE G_TEST TO PUBLIC
"""

expected_stdout_6x = """
    /* Grant permissions for this database */
    GRANT SELECT ON PUBLIC.TEST TO PUBLIC
    GRANT USAGE ON SCHEMA PUBLIC TO PUBLIC

    /* Grant permissions for this database */
    GRANT SELECT ON PUBLIC.TEST TO PUBLIC
    GRANT USAGE ON SEQUENCE PUBLIC.G_TEST TO PUBLIC
    GRANT USAGE ON SCHEMA PUBLIC TO PUBLIC
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

