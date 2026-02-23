#coding:utf-8

"""
ID:          9d708e4742
ISSUE:       https://www.sqlite.org/src/tktview/9d708e4742
TITLE:       Assertion
DESCRIPTION:
NOTES:
    [15.08.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md
    Checked on 6.0.0.1204, 5.0.4.1701, 4.0.7.3231, 3.0.14.33824
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table t0(c0 boolean, check(case c0 when c0 then false end));
    create table t1(c0 smallint, check(case c0 when c0 then false end));
    set count on;
    insert into t0 values(true);
    insert into t0 values(false);
    insert into t1 values(1);
    insert into t1 values(0);
"""

substitutions = [('[ \t]+', ' ')]

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

# NB: these two pairs must fire *after* removing schema name and quotes:
substitutions.extend( [("At trigger .*", "At trigger"), ('constraint INTEG_\\d+ .*', 'constraint INTEG')] )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    Statement failed, SQLSTATE = 23000
    Operation violates CHECK constraint INTEG_1 on view or table T0
    -At trigger 'CHECK_1'
    Records affected: 0

    Statement failed, SQLSTATE = 23000
    Operation violates CHECK constraint INTEG_1 on view or table T0
    -At trigger 'CHECK_1'
    Records affected: 0

    Statement failed, SQLSTATE = 23000
    Operation violates CHECK constraint INTEG_2 on view or table T1
    -At trigger 'CHECK_3'
    Records affected: 0

    Statement failed, SQLSTATE = 23000
    Operation violates CHECK constraint INTEG_2 on view or table T1
    -At trigger 'CHECK_3'
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
