#coding:utf-8

"""
ID:          issue-1279
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1279
TITLE:       Ability to query a stored procedur from view.
DESCRIPTION:
JIRA:        CORE-886
FBTEST:      bugs.core_0886
NOTES:
    [23.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set blob all;
    set term ^;
    create procedure MY_PROCEDURE (input1 INTEGER)
    returns (output1 INTEGER) as
    begin
        output1 = input1+1;
        suspend;
    end ^
    set term ;^
    commit;

    create view a_view as
    select * from MY_PROCEDURE(1);
    commit;

    select rdb$view_source as blob_id from rdb$relations where rdb$relation_name = upper('A_VIEW');
    select * from a_view;
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [('[ \t]+', ' '), ('BLOB_ID.*', '')]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout = """
    select * from MY_PROCEDURE(1)
    OUTPUT1 2
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

