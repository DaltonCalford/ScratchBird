#coding:utf-8

"""
ID:          issue-384
ISSUE:       384
TITLE:       Automatic not null in PK columns incomplete
DESCRIPTION:
JIRA:        CORE-59
FBTEST:      bugs.core_0059
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
    recreate table test (a int, b float, c varchar(10), primary key (a, b, c));
    commit;
    insert into test(a) values(null);
    insert into test(a,b) values(1,null);
    insert into test(a,b,c) values(1,1,null);
    insert into test default values;
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [ ('line: \\d+, col: \\d+', '') ]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stderr = """
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."A", value "*** null ***"
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."B", value "*** null ***"
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."C", value "*** null ***"
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST"."A", value "*** null ***"
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

