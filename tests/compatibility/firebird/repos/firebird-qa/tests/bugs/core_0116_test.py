#coding:utf-8

"""
ID:          issue-438
ISSUE:       438
TITLE:       BLOBs in external tables
DESCRIPTION:
JIRA:        CORE-116
FBTEST:      bugs.core_0116
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
    -- Untill build 3.0.0.31721 STDERR was:
    -- -Data type BLOB is not supported ... field '<Missing arg #3 - possibly status vector overflow>'
    -- Since WI-T3.0.0.31733 STDERR became normal - contains name of field.
    -- See correction in 'expected_stderr' secsion (23.03.2015).
    create table ext_log external file '$(DATABASE_LOCATION)z.dat' (F1 INT, F2 BLOB SUB_TYPE TEXT);
"""

# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions = [ ('line: \\d+, col: \\d+', '') ]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = isql_act('db', test_script, substitutions = substitutions)

expected_stderr = """
    Statement failed, SQLSTATE = HY004
    unsuccessful metadata update
    -CREATE TABLE EXT_LOG failed
    -SQL error code = -607
    -Invalid command
    -Data type BLOB is not supported for EXTERNAL TABLES. Relation 'EXT_LOG', field 'F2'
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

