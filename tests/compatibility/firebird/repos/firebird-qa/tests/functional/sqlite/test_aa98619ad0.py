#coding:utf-8

"""
ID:          aa98619ad0
ISSUE:       https://www.sqlite.org/src/tktview/aa98619ad0
TITLE:       Assertion fault on an IN operator using a computed-by index
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    FB 5.x+ has optimized processing of IN <list> predicates, see:
    https://github.com/FirebirdSQL/firebird/pull/7707
    Execution plan for FB 5.x+ will have only one occurrence of 'INDEX (<idx_name>)'.
    Test verifies only 5.x+ versions.

    Checked on 6.0.0.1204, 5.0.4.1701
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1 (x char(1));
    create index i1 on t1 computed by( upper(x) );
    set plan on;
    set count on;
    select 1 from t1 dfs where upper(x)=1 and upper(x) in ('a', 'b', 'c');
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
    PLAN (DFS INDEX (I1))
    Records affected: 0
"""

@pytest.mark.version('>=5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
