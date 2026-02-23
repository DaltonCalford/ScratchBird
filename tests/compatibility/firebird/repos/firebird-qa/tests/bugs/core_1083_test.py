#coding:utf-8

"""
ID:          issue-1504
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1504
TITLE:       User (not SYSDBA) that have privileges with grant option, can't revoke privileges, granted by other user or SYSDBA
DESCRIPTION:
JIRA:        CORE-1083
FBTEST:      bugs.core_1083
NOTES:
    [23.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_user = user_factory('db', name='tmp$c1083', password='123')
tmp_role = role_factory('db', name='dba_helper')

substitutions = []

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', substitutions=substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_user: User, tmp_role: Role):

    test_sql = f"""
        recreate table tab1(col1 integer);
        recreate table tab2(col2 integer);
        commit;

        grant update (col1) on tab1 to {tmp_user.name} with grant option;
        grant update (col2) on tab2 to {tmp_role.name};
        commit;

        connect '{act.db.dsn}' user '{tmp_user.name}' password '{tmp_user.password}';
        set echo on;
        grant update(col1) on tab1 to {tmp_role.name};
        revoke update(col1) on tab1 from {tmp_role.name};
        revoke update(col2) on tab2 from {tmp_role.name};
    """

    expected_stdout = f"""
        grant update(col1) on tab1 to {tmp_role.name.upper()};
        revoke update(col1) on tab1 from {tmp_role.name.upper()};
        revoke update(col2) on tab2 from {tmp_role.name.upper()};
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -REVOKE failed
        -{tmp_user.name} is not grantor of UPDATE on TAB2 to {tmp_role.name.upper()}.
    """

    act.expected_stdout = expected_stdout
    act.isql(switches = ['-q'], input = test_sql, combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

