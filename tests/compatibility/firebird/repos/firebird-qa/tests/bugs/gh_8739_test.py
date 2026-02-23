#coding:utf-8

"""
ID:          issue-8739
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8739
TITLE:       Wrong SQLSTATE in case of table alias conflict
DESCRIPTION:
NOTES:
    [15.09.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.1275-402365e; 5.0.4.1713-e89e627; 3.0.14.33824-f594ddf
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t1(x int, y int);
    recreate table t2(x int, y int);
    recreate table t3(x int, y int);

    set list on;
    set plan on;
    set count on;
    select *
    from t1 a
    join t1 b on a.x = b.x
    join t1 b on a.x = b.x
    ;
    -------------------------------------------------------------
    select *
    from t1 as t2
    join t2 as t1 on t1.x = t2.y
    join t3 as t2 on t2.x = t3.y
    ;
"""

substitutions = [('[ \t]+', ' ')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -204
    -alias B conflicts with an alias in the same statement

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -204
    -alias T2 conflicts with an alias in the same statement
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
