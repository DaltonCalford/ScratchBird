#coding:utf-8

"""
ID:          3dbdcdb14e
ISSUE:       https://www.sqlite.org/src/tktview/3dbdcdb14e
TITLE:       Assertion fault using indices with redundant columns
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
    create table test(f01 int, f02 int);
    create index test_idx on test(f01,f02,f01);
    commit;
    select count(*) from rdb$indices where rdb$index_name = upper('test_idx');
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
    -CREATE INDEX TEST_IDX failed
    -Field F01 cannot be used twice in index TEST_IDX

    COUNT 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
