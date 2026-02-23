#coding:utf-8

"""
ID:          issue-1257
ISSUE:       1257
TITLE:       Removing a NOT NULL constraint is not visible until reconnect
DESCRIPTION:
JIRA:        CORE-866
FBTEST:      bugs.core_0866
NOTES:
    [23.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *


test_script = """
    recreate table test (
        id integer not null,
        col varchar(20) not null
    );
    insert into test (id, col) values (1, 'data');
    commit;
    update rdb$relation_fields
      set rdb$null_flag = null
      where (rdb$field_name = upper('col')) and (rdb$relation_name = upper('test'));
    commit;

    update test set col = null where id = 1;
"""

db = db_factory()

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [('[ \t]+', ' ')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 42000
    UPDATE operation is not allowed for system table RDB$RELATION_FIELDS
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."COL", value "*** null ***"
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

