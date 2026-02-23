#coding:utf-8

"""
ID:          f8f472cbc7
ISSUE:       https://www.sqlite.org/src/tktview/f8f472cbc7
TITLE:       Partial index and BETWEEN issue
DESCRIPTION:
NOTES:
    [18.08.2025] pzotov
    ::: NB :::
    FB issues opposite results when comparing NULL and FALSE:
        * select '' between null and 1 from rdb$database ==> SQLSTATE = 22018 / conversion error from string ""
        * select '' between null and '1' from rdb$database ==> null
        * select ('' between null and '1') in (false) from rdb$database ==> null
    Current test expressions do not match to the original one.

    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0 (id int generated always as identity, c0 char(1));
    create index i0 on t0 computed by('1') where c0 is not null;
    insert into t0(c0) values (null);
    insert into t0(c0) values ('');

    set count on;
    set plan on;
    select t0.*, ('' between t0.c0 and '1') in (null,false) as chk from t0 where c0 is null;
    select t0.*, ('' between t0.c0 and '1') in (null,false) as chk from t0 where c0 is not null;
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
    PLAN (T0 NATURAL)
    ID 1
    C0 <null>
    CHK <null>
    Records affected: 1

    PLAN (T0 INDEX (I0))
    ID 2
    C0
    CHK <null>
    Records affected: 1
"""

@pytest.mark.version('>=5')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
