#coding:utf-8

"""
ID:          issue-1475
ISSUE:       1475
TITLE:       A query could produce different results, depending on the presence of an index
DESCRIPTION:
JIRA:        CORE-1056
FBTEST:      bugs.core_1056
NOTES:
    [24.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.858; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """

    set list on;
    create table t (c varchar(10) character set win1250 collate pxw_csy);
    insert into t values ('ch');
    commit;

    set plan on;
    select * from t where c starting with 'c';
    commit;

    create index t_c on t (c);
    commit;

    select * from t where c starting with 'c';
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

expected_stdout = """
    PLAN (T NATURAL)
    C ch
    PLAN (T INDEX (T_C))
    C ch
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
