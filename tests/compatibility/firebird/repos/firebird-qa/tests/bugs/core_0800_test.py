#coding:utf-8

"""
ID:          issue-1186
ISSUE:       1186
TITLE:       Easy metadata extract improvements
DESCRIPTION: Domain DDL: move its CHECK clause from 'create' to 'alter' statement.
JIRA:        CORE-800
FBTEST:      bugs.core_0800
NOTES:
    [24.06.2025] pzotov
    FB-6.x snapshot must be 6.0.0.854-10b585b or newer,
    see: #8622 (Regression: ISQL crashes on attempt to extract metadata when domain with reference to user-defined collation presents.)
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.858; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^;
    execute block as
    begin
      begin
        execute statement 'drop domain dm_test';
        when any do begin end
      end
      begin
        execute statement 'drop collation name_coll';
        when any do begin end
      end
    end^
    set term ;^
    commit;

    create collation name_coll for utf8 from unicode no pad case insensitive accent insensitive;
    commit;

    create domain dm_test varchar(20)
       character set utf8
       default 'foo'
       not null
       check (value in ('foo', 'rio', 'bar'))
       collate name_coll
       ;
    commit;
"""

db = db_factory(charset='UTF8', init=init_script)

substitutions = []
# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = python_act('db', substitutions = substitutions)

expected_stdout = """
    ALTER DOMAIN DM_TEST ADD CONSTRAINT
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.extract_meta()
    expected = ''.join([x for x in act.clean_stdout.splitlines() if 'ALTER DOMAIN' in x.upper()])
    assert act.clean_expected_stdout == expected


