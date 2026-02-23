#coding:utf-8

"""
ID:          issue-1588
ISSUE:       1588
TITLE:       WHEN <list of exceptions> tracks only the dependency on the first exception in PSQL
DESCRIPTION:
JIRA:        CORE-1165
FBTEST:      bugs.core_1165
NOTES:
    [24.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.858; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate exception exc_1 'exc_1' ;
    recreate exception exc_2 'exc_2' ;

    set term ^;

    create procedure sp_test as
    begin
      begin end
      when exception exc_1, exception exc_2 do
      begin
      end
    end
    ^
    set term ;^
    commit;

    select rd.rdb$depended_on_name depends_on_name
    from rdb$dependencies rd
    where upper(rd.rdb$dependent_name) = upper('sp_test')
    order by depends_on_name
    ;
    commit;

    recreate exception exc_1 'exc_1';
    recreate exception exc_2 'exc_2';
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
    DEPENDS_ON_NAME EXC_1
    DEPENDS_ON_NAME EXC_2

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -EXCEPTION EXC_1
    -there are 1 dependencies

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -EXCEPTION EXC_2
    -there are 1 dependencies
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

