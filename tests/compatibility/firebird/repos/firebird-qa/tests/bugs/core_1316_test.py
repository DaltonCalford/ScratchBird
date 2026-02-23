#coding:utf-8

"""
ID:          issue-1735
ISSUE:       1735
TITLE:       NOT NULL constraint for procedure parameters and variables
DESCRIPTION:
JIRA:        CORE-1316
FBTEST:      bugs.core_1316
NOTES:
    [24.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Adjusted explained plan in 6.x to actual.

    Checked on 6.0.0.858; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create procedure get_something(id integer not null) as begin end;
    commit;
    execute procedure get_something(NULL);
    execute procedure get_something(1);
    set term ^;
    create procedure sp_test(inp int) as declare i int not null; begin i = inp; end^
    set term ;^
    commit;
    execute procedure sp_test(null);
    execute procedure sp_test(1);
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [('line: \\d+, col: \\d+', '')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    validation error for variable ID, value "*** null ***"
    -At procedure 'GET_SOMETHING'

    Statement failed, SQLSTATE = 42000
    validation error for variable I, value "*** null ***"
    -At procedure 'SP_TEST' line: 1, col: 63
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

