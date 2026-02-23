#coding:utf-8

"""
ID:          issue-1236
ISSUE:       1236
TITLE:       Computed field can't be changed to non-computed using 'alter table alter column type xy'
DESCRIPTION:
JIRA:        CORE-847
FBTEST:      bugs.core_0847
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
    set blob off;
    set list on;

    recreate table t (
      f1 smallint,
      f2 smallint,
      sum_f1_f2 computed by (f1+f2)
    );

    insert into t (f1,f2) values (1,2);
    commit;

    select f1,f2,sum_f1_f2 as cf_before_altering from t;

    select b.rdb$field_name field_name, cast(a.rdb$computed_source as varchar(80)) computed_source_before_altering
    from rdb$fields a
    join rdb$relation_fields b  on a.rdb$field_name = b.rdb$field_source
    where b.rdb$field_name = upper('SUM_F1_F2');

    alter table t alter sum_f1_f2 type bigint;
    commit;

    select f1,f2,sum_f1_f2 as cf_after_altering from t;

    select b.rdb$field_name field_name, cast(a.rdb$computed_source as varchar(80)) computed_source_after_altering
    from rdb$fields a
    join rdb$relation_fields b  on a.rdb$field_name = b.rdb$field_source
    where b.rdb$field_name = upper('SUM_F1_F2');
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
    F1 1
    F2 2
    CF_BEFORE_ALTERING 3
    FIELD_NAME SUM_F1_F2
    COMPUTED_SOURCE_BEFORE_ALTERING (f1+f2)
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TABLE T failed
    -Cannot add or remove COMPUTED from column SUM_F1_F2
    F1 1
    F2 2
    CF_AFTER_ALTERING 3
    FIELD_NAME SUM_F1_F2
    COMPUTED_SOURCE_AFTER_ALTERING (f1+f2)
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

