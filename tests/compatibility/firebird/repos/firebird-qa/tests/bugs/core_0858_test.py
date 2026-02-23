#coding:utf-8

"""
ID:          issue-1248
ISSUE:       1248
TITLE:       Server crash when using UDF
DESCRIPTION:
NOTES:
[24.01.2019] Disabled this test to be run on FB 4.0
  UDF usage is deprecated in FB 4+, see: ".../doc/README.incompatibilities.3to4.txt".
  Functions div, frac, dow, sdow, getExactTimestampUTC and isLeapYear got safe replacement
  in UDR library "udf_compat", see it in folder: ../plugins/udr/
  UDF function 'sright' has direct built-in analog 'right', there is no UDR function for it.
JIRA:        CORE-858
FBTEST:      bugs.core_0858
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    declare external function sright
    varchar(100) by descriptor, smallint,
    varchar(100) by descriptor returns parameter 3
    entry_point 'right' module_name 'fbudf';

    commit;

    set list on;
    select
        rdb$function_name
        ,rdb$function_type
        ,rdb$module_name
        ,rdb$entrypoint
        ,rdb$return_argument
        ,rdb$system_flag
        ,rdb$legacy_flag
    from rdb$functions where upper(rdb$function_name) = upper('sright');

    select sright('qwerty', 2) as sright_result from rdb$database;
    commit;

    drop external function sright;
    commit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RDB$FUNCTION_NAME               SRIGHT
    RDB$FUNCTION_TYPE               <null>
    RDB$MODULE_NAME                 fbudf
    RDB$ENTRYPOINT                  right
    RDB$RETURN_ARGUMENT             3
    RDB$SYSTEM_FLAG                 0
    RDB$LEGACY_FLAG                 1

    SRIGHT_RESULT                   ty
"""

@pytest.mark.version('>=3.0,<4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
