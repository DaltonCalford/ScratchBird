#coding:utf-8

"""
ID:          dd08e5a988
ISSUE:       https://www.sqlite.org/src/tktview/dd08e5a988
TITLE:       Foreign key constraint fails to prevent consistency error.
DESCRIPTION:
NOTES:
    [22.08.2025] pzotov
    Checked on 6.0.0.1244, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(a integer primary key, b int, unique(a,b));
    create table t2(w int,x int,y int, constraint t2_fk foreign key(x,y) references t1(a,b));

    insert into t1 values(100,200);
    insert into t2 values(300,100,200);
    set count on;
    delete from t1;
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
    Statement failed, SQLSTATE = 23000
    violation of FOREIGN KEY constraint "T2_FK" on table "T2"
    -Foreign key references are present for the record
    -Problematic key value is ("A" = 100, "B" = 200)
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
