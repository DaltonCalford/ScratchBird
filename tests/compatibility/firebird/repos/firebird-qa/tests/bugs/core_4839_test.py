#coding:utf-8

"""
ID:          issue-5135
ISSUE:       5135
TITLE:       SHOW GRANTS does not display info about exceptions which were granted to user
DESCRIPTION:
JIRA:        CORE-4839
FBTEST:      bugs.core_4839
NOTES:
    [15.05.2025] pzotov
    Added substitutions in order to suppress excessive lines produced by 'SHOW GRANTS':
    they may remain after some failed test teardown phases.

    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

tmp_user = user_factory('db', name='tmp$c4839', password='123')

test_script = """
    recreate exception exc_foo 'Houston we have a problem: next sequence value is @1';
    recreate sequence gen_bar start with 9223372036854775807 increment by 2147483647;
    commit;

    grant usage on exception exc_foo to tmp$c4839; -- this wasn`t shown before rev. 61822 (build 3.0.0.31881), 2015-06-14 11:35
    grant usage on sequence gen_bar to tmp$c4839;
    commit;
    show grants;
    commit;
"""

substitutions = [('^((?!USAGE ON (SEQUENCE|EXCEPTION)).)*$', '')]

act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):
    expected_stdout_5x = f"""
        GRANT USAGE ON SEQUENCE GEN_BAR TO USER {tmp_user.name.upper()}
        GRANT USAGE ON EXCEPTION EXC_FOO TO USER {tmp_user.name.upper()}
    """

    expected_stdout_6x = f"""
        GRANT USAGE ON SEQUENCE PUBLIC.GEN_BAR TO USER {tmp_user.name.upper()}
        GRANT USAGE ON EXCEPTION PUBLIC.EXC_FOO TO USER {tmp_user.name.upper()}
    """
    
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
