#coding:utf-8

"""
ID:          computed-fields-16
FBTEST:      functional.gtcs.computed_fields_16
TITLE:       Computed fields
DESCRIPTION:
  Original test see in:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/CF_ISQL_16.script
  SQL script for creating test database ('gtcs_sp1.fbk') and fill it with some data:
  https://github.com/FirebirdSQL/fbtcs/blob/master/GTCS/tests/PROCS_QA_INIT_ISQL.script
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    /*-------------------------------------------------------------*/
    /* Create a table with computed field and improper attributes. */
    /*-------------------------------------------------------------*/

    recreate table t0 (a integer, af computed by (a*3) default 10);

    recreate table t1 (a integer, af computed by (a*3) not null);

    recreate table t2 (a char(5), af computed by (a||a) collate DOS850);

    recreate table t3 (a integer, af computed by (a*3) check (a > 3));

    recreate table t4 (a integer primary key);

    recreate table t4r (a integer, af computed by (a*3) references t4(a));


    recreate table t5 (a integer, af computed by (a*3) unique);

    recreate table t6 (a integer, af computed by (a*3) primary key);


"""

act = isql_act('db', test_script, substitutions = [('line\\s+\\d+(,)?\\s+col(umn)?\\s+\\d+', '')])

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 52
    -default

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 52
    -not

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 53
    -collate

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 52
    -check

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 53
    -references

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 52
    -unique

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -Token unknown - line 1, column 52
    -primary
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr
