#coding:utf-8

"""
ID:          issue-1813
ISSUE:       1813
TITLE:       Few problems with domains's check constraints
DESCRIPTION:
JIRA:        CORE-1395
FBTEST:      bugs.core_1395
NOTES:
    [24.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.858; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    create table test ( id integer );
    create domain test_domain as integer
        check ( exists(select * from test where id=value) );
"""

db = db_factory(init=init_script)

test_script = """
    drop table test;
    commit;
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = []
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN TEST.ID
    -there are 1 dependencies
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

