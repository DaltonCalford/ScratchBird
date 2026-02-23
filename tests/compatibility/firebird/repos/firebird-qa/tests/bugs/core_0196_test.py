#coding:utf-8

"""
ID:          issue-523
ISSUE:       523
TITLE:       SYSDBA can grant non existent roles
DESCRIPTION:
JIRA:        CORE-196
FBTEST:      bugs.core_0190
NOTES:
    [22.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    grant no_such_role to tmp$c0196;
    commit;
    set count on;
    set list on;
    select * from rdb$user_privileges where rdb$user = upper('tmp$c0196') rows 1;
    commit;
"""

substitutions = [('Statement failed, SQLSTATE = HY000', ''), ('record not found for user:.*', '')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [ ('line: \\d+, col: \\d+', '') ]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Records affected: 0
"""

expected_stderr = """
    Statement failed, SQLSTATE = 28000
    unsuccessful metadata update
    -GRANT failed
    -SQL role NO_SUCH_ROLE does not exist
"""

tmp_user = user_factory('db', name='tmp$c0196', password='123')

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

