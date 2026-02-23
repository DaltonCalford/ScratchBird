#coding:utf-8

"""
ID:          issue-1444
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1444
TITLE:       Bad plan in outer joins with IS NULL clauses (dependent on order of predicates)
DESCRIPTION:
JIRA:        CORE-1029
FBTEST:      bugs.core_1029
NOTES:
    [24.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table tb1 (id int, col int) ;

    insert into tb1 values (1, 1) ;
    insert into tb1 values (2, 2) ;
    insert into tb1 values (1, null) ;
    commit;
    create index tbi1 on tb1 (id);
    create index tbi2 on tb1 (col);
    commit;

    set planonly;
    select * from tb1 a
    left join tb1 b on a.id = b.id
    where a.col is null and a.col+0 is null;

    select * from tb1 a
    left join tb1 b on a.id = b.id
    where a.col+0 is null and a.col is null;
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
    PLAN JOIN (A INDEX (TBI2), B INDEX (TBI1))
    PLAN JOIN (A INDEX (TBI2), B INDEX (TBI1))
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
