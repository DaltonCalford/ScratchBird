#coding:utf-8

"""
ID:          issue-613
ISSUE:       613
TITLE:       DOMAINs don't register their dependency on other objects
DESCRIPTION:
JIRA:        CORE-282
FBTEST:      bugs.core_0282
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
    set list on;
    create table test(f01 int);
    create domain dm_int int check(value > (select max(f01) from test));
    commit;
    drop table test;
    commit;
    create table test2(f01 dm_int);
    commit;
    show table test2;
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [('[ \t]+', ' ')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN TEST.F01
    -there are 1 dependencies
    F01 (DM_INT) INTEGER Nullable
    check(value > (select max(f01) from test))
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN TEST.F01
    -there are 1 dependencies
    Table: TEST2
    F01 (DM_INT) INTEGER Nullable
    check(value > (select max(f01) from test))
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
