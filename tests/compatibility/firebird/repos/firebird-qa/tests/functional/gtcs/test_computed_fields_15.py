#coding:utf-8

"""
ID:          computed-fields-15
FBTEST:      functional.gtcs.computed_fields_15
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_15.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set heading off;
    /*-----------------------------------------------------------------------------*/
    /* Create a table with computed field which is defined using non-existing UDF. */
    /*-----------------------------------------------------------------------------*/
    create table t0 (a integer, af computed by ( non_exist_udf(a) ));
"""

act = isql_act('db', test_script, substitutions=[('^((?!Statement failed|SQL error code).)*$', ''),
                                                 (' = ', ' '), ('[ \t]+', ' ')])

expected_stderr = """
    Statement failed, SQLSTATE 39000
    Dynamic SQL Error
    -SQL error code -804
    -Function unknown
    -NON_EXIST_UDF
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
