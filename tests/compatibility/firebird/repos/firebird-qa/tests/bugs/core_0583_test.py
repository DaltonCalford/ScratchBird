#coding:utf-8

"""
ID:          issue-939
ISSUE:       939
TITLE:       Before triggers are firing after checks
DESCRIPTION:
JIRA:        CORE-583
FBTEST:      bugs.core_0583
NOTES:
    [22.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test1 (i int, constraint test1_chk check (i between 1 and 5));
    commit;

    set term ^;
    create trigger test1_bi for test1 active before insert position 0 as
    begin
       new.i=6;
    end
    ^

    create trigger test1_bu for test1 active before update position 0 as
    begin
       new.i=7;
    end
    ^
    set term ;^
    commit;

    set count on;
    insert into test1 values (2);
    select * from test1;
    update test1 set i=2 where i = 6;
    select * from test1;
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [('[ \t]+', ' '), ('(-)?At trigger.*', 'At trigger')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)


expected_stdout = """
    Statement failed, SQLSTATE = 23000
    Operation violates CHECK constraint TEST1_CHK on view or table TEST1
    At trigger
    Records affected: 0
    Records affected: 0
    Records affected: 0
    Records affected: 0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
