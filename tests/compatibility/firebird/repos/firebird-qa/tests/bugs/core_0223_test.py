#coding:utf-8

"""
ID:          issue-554
ISSUE:       554
TITLE:       ALTER TABLE altering to VARCHAR
DESCRIPTION:
JIRA:        CORE-223
FBTEST:      bugs.core_0223
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
    set list on;
    recreate table test1(x int);
    --create index test1_x on test1(x);

    insert into test1 values(2000000000);
    insert into test1 values(100000000);
    insert into test1 values(50000000);
    commit;

    select * from test1 order by x;
    commit;

    alter table test1 alter x type varchar(5);
    alter table test1 alter x type varchar(9);

    alter table test1 alter x type varchar(11);

    -- Here values must be sorted as TEXT:
    select * from test1 order by x;
    commit;
"""

substitutions = []
# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [ ('line: \\d+, col: \\d+', '') ]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)


expected_stdout = """
    X                               50000000
    X                               100000000
    X                               2000000000

    X                               100000000
    X                               2000000000
    X                               50000000
"""
expected_stderr = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TABLE TEST1 failed
    -New size specified for column X must be at least 11 characters.

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TABLE TEST1 failed
    -New size specified for column X must be at least 11 characters.
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

