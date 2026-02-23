#coding:utf-8

"""
ID:          tabloid.optimizer-index-navigation
TITLE:       Check that optimizer takes in account presense of index and does navigation instead of external sort.
DESCRIPTION: 
  Verified commit: https://github.com/FirebirdSQL/firebird/actions/runs/176006556
      Source message to dimitr: 20.07.2020 20:00.
  
      Checked on 3.0.7.33340 and 4.0.0.2114 (intermediate build with timestamp 20.07.2020 17:45)
FBTEST:      functional.tabloid.optimizer_index_navigation
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table t(x int);
    create index t_x_asc on t(x);
    create descending index t_x_dec on t(x);
    insert into t select rand()*1000 from rdb$types,rdb$types;
    commit;
    set statistics index t_x_asc;
    set statistics index t_x_dec;
    commit;
    set planonly;
     
    select * from t as t1 where x>=0.5; -- NO 'plan order' must be here; bitmap is enough!

    select * from t as t2 where x>=0.5 order by x; -- here PLAN ORDER is much efficient than bitmap + PLAN SORT

    select * from t as t3 where x<=0.5; -- NO 'plan order' must be here; bitmap is enough!

    select * from t as t4 where x<=0.5 order by x desc; -- here PLAN ORDER is much efficient than bitmap + PLAN SORT
"""


substitutions = []

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    PLAN (T1 INDEX (T_X_ASC))
    PLAN (T2 ORDER T_X_ASC)
    PLAN (T3 INDEX (T_X_ASC))
    PLAN (T4 ORDER T_X_DEC)
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
