#coding:utf-8

"""
ID:          b5ca442af9
ISSUE:       https://www.sqlite.org/src/tktview/b5ca442af9
TITLE:       "Malformed database schema" when creating a failing index within a transaction
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    On FB 3.x error message does not contain line "Expression evaluation error for index ***unknown*** on table TEST"
    Test does not checks this version.

    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test(c0 bigint);
    commit;
    insert into test(c0) values (-9223372036854775808);
    create index test_idx on test computed by ( ln(c0) );
    commit;
    drop index test_idx;
    create index test_idx on test computed by(c0 - 1);
    commit;
    set count on;
    select * from rdb$indices where rdb$relation_name = upper('test');
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
    Statement failed, SQLSTATE = 40001
    lock conflict on no wait transaction
    -unsuccessful metadata update
    -object TABLE "TEST" is in use
    
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -DROP INDEX TEST_IDX failed
    -Index not found
    
    Statement failed, SQLSTATE = 22003
    Expression evaluation error for index "***unknown***" on table "TEST"
    -Integer overflow. The result of an integer operation caused the most significant bit of the result to carry.
    
    Records affected: 0
"""

@pytest.mark.version('>=4')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
