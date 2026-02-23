#coding:utf-8

"""
ID:          gtcs.ref_integ_drop_pk_constraint
TITLE:       Constraint of PRIMARY KEY should not be avail for DROP if there is FK that depends on it
DESCRIPTION:
    Original test see in:
    https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/REF_INT.2.ISQL.script
FBTEST:      functional.gtcs.ref_integ_drop_pk_constraint
NOTES:
    [07.08.2024] pzotov
    Splitted expected* text because system triggers now are created in C++/GDML code
    See https://github.com/FirebirdSQL/firebird/pull/8202
    Commit (05-aug-2024 13"45):
    https://github.com/FirebirdSQL/firebird/commit/0cc8de396a3c2bbe13b161ecbfffa8055e7b4929

    [11.07.2025] pzotov
    Increased the 'subsitutions' list to suppress "PUBLIC" schema prefix and remove single/double quotes from object names. Need since 6.0.0.834.
    ::: NB :::
    File act.files_dir/'test_config.ini' must contain section:
        [schema_n_quotes_suppress]
        addi_subst="PUBLIC". " '
    (thi file is used in qa/plugin.py, see QA_GLOBALS dictionary).

    Value of parameter 'addi_subst' is splitted on tokens using space character and we add every token to 'substitutions' list which
    eventually will be like this:
        substitutions = [ ( <previous tuple(s)>, ('"PUBLIC".', ''), ('"', ''), ("'", '') ]
"""

import os
import pytest
from firebird.qa import *

db = db_factory()

test_expected_stdout = """
    Records affected: 0
"""

expected_stderr_5x = """
    Statement failed, SQLSTATE = 27000
    unsuccessful metadata update
    -DROP INDEX DEPT_KEY failed
    -action cancelled by trigger (1) to preserve data integrity
    -Cannot delete index used by an Integrity Constraint

    Statement failed, SQLSTATE = 23000
    violation of PRIMARY or UNIQUE KEY constraint "DEPT_KEY" on table "DEPARTMENT"
    -Problematic key value is ("DEPT_NO" = 1)
"""

expected_stderr_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -DROP INDEX DEPT_KEY failed
    -Cannot delete index used by an Integrity Constraint

    Statement failed, SQLSTATE = 23000
    violation of PRIMARY or UNIQUE KEY constraint "DEPT_KEY" on table "DEPARTMENT"
    -Problematic key value is ("DEPT_NO" = 1)
"""


# QA_GLOBALS -- dict, is defined in qa/plugin.py, obtain settings
# from act.files_dir/'test_config.ini':
#
addi_subst_settings = QA_GLOBALS['schema_n_quotes_suppress']
addi_subst_tokens = addi_subst_settings['addi_subst']

substitutions=[('[ \t]+', ' '),]
for p in addi_subst_tokens.split(' '):
    substitutions.append( (p, '') )

act = python_act('db', substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    
    sql_init = (act.files_dir / 'gtcs-ref-integ-init.sql').read_text()
    sql_addi = '''
        drop index dept_key;
        -- Check that PK index still in use: following must FAIL:
        set count on;
        insert into department( dept_no, dept_name) values (1, 'k1');
    '''

    act.expected_stdout = test_expected_stdout
    act.expected_stderr = expected_stderr_5x if act.is_version('<6') else expected_stderr_6x
   
    act.isql(switches=['-q'], input = os.linesep.join( (sql_init, sql_addi) ) )

    assert (act.clean_stdout == act.clean_expected_stdout and
            act.clean_stderr == act.clean_expected_stderr)
