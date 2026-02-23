#coding:utf-8

"""
ID:          issue-1511
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1511
TITLE:       FK-definition. Make error message more relevant when parent table has no appropriate PK/UK constraint.
DESCRIPTION:
JIRA:        CORE-1090
FBTEST:      bugs.core_1090
NOTES:
    [24.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Previous test title: Error msg "Could not find UNIQUE INDEX" when in fact one is present [CORE1090]
    Checked on 6.0.0.858; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table tmain(id int not null);
    create unique index tmain_id_unq on tmain(id);
    create table tdetl(pid int references tmain(id));
"""

substitutions = []

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
    -CREATE TABLE TDETL failed
    -could not find UNIQUE or PRIMARY KEY constraint in table TMAIN with specified columns
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

