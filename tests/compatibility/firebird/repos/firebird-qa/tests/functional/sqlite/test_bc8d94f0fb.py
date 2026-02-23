#coding:utf-8

"""
ID:          bc8d94f0fb
ISSUE:       https://www.sqlite.org/src/tktview/bc8d94f0fb
TITLE:       RENAME COLUMN fails on tables with redundant UNIQUE constraints
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    -- Must issue: sqlstate 42000 / same set of columns cannot be used in more 
    -- than one primary key and/or unique constraint definition
    recreate table t1(aaa int, unique(aaa), unique(aaa), unique(aaa), check(aaa>0));

    recreate table t2(bbb int unique);
    -- Must issue: sqlstate 42000 / same set of columns cannot be used in more 
    -- than one primary key and/or unique constraint definition
    alter table t2 add constraint t2_unq_add unique(bbb);
    
    alter table t1 alter column aaa to bbb;
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
    unsuccessful metadata update
    -RECREATE TABLE T1 failed
    -Same set of columns cannot be used in more than one PRIMARY KEY and/or UNIQUE constraint definition

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TABLE T2 failed
    -Same set of columns cannot be used in more than one PRIMARY KEY and/or UNIQUE constraint definition
    
    Statement failed, SQLSTATE = 42S02
    unsuccessful metadata update
    -ALTER TABLE T1 failed
    -SQL error code = -607
    -Invalid command
    -Table T1 does not exist
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
