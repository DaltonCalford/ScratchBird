#coding:utf-8

"""
ID:          issue-428
ISSUE:       428
TITLE:       Dropping and recreating a table in the same txn disables PK
DESCRIPTION:
JIRA:        CORE-104
FBTEST:      bugs.core_0104
NOTES:
    [22.06.2025] pzotov
    ::: NB :::
    SQL schema name (introduced since 6.0.0.834), single and double quotes are suppressed in the output.
    See $QA_HOME/README.substitutions.md or https://github.com/FirebirdSQL/firebird-qa/blob/master/README.substitutions.md

    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """create table test (acolumn int not null primary key);
commit;
"""

db = db_factory(init=init_script)

test_script = """SET AUTODDL OFF;

drop table test;
create table test (acolumn int not null primary key);

commit;

insert into test values (1);
insert into test values (1);

commit;
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [ ('line: \\d+, col: \\d+', ''), ('INTEG_\\d+', 'INTEG') ]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stderr = """Statement failed, SQLSTATE = 23000
violation of PRIMARY or UNIQUE KEY constraint "INTEG_4" on table "TEST"
-Problematic key value is ("ACOLUMN" = 1)
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

