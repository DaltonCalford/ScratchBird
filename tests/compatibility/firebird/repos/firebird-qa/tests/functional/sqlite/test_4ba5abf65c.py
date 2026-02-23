#coding:utf-8

"""
ID:          4ba5abf65c
ISSUE:       https://www.sqlite.org/src/tktview/4ba5abf65c
TITLE:       Index on expression leads to an incorrect LEFT JOIN 
DESCRIPTION:
NOTES:
    [20.08.2025] pzotov
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t1(x varchar(10));
    create table t2(y varchar(10), z int);
    insert into t1 values('key');
    insert into t2 values('key', -1);

    set count on;
    set plan on;
    select count(*) from t1 left join t2 on t1.x = t2.y where y || coalesce(z, 0) >= '';
    commit;
    create index t2i on t2 computed by ( y || coalesce(z, 0) );
    select count(*) from t1 left join t2 on t1.x = t2.y where y || coalesce(z, 0) >= '';
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

@pytest.mark.version('>=4')
def test_1(act: Action):

    expected_stdout_4x = """
        PLAN JOIN (T1 NATURAL, T2 NATURAL)
        COUNT 1
        Records affected: 1
        PLAN JOIN (T1 NATURAL, T2 NATURAL)
        COUNT 1
        Records affected: 1
    """

    expected_stdout_5x = """
        PLAN HASH (T1 NATURAL, T2 NATURAL)
        COUNT 1
        Records affected: 1
        PLAN HASH (T1 NATURAL, T2 INDEX (T2I))
        COUNT 1
        Records affected: 1
    """

    act.expected_stdout = expected_stdout_4x if act.is_version('<5') else expected_stdout_5x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
