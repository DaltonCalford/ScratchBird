#coding:utf-8

"""
ID:          issue-6426
ISSUE:       6426
TITLE:       Operations when using "SET DECFLOAT BIND BIGINT,n" with result of 11+ digits,
  fail with "Decimal float invalid operation"
DESCRIPTION:
JIRA:        CORE-6181
FBTEST:      bugs.core_6181
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    -- OLD SYNTAX: set decfloat bind bigint,3;
    -- Syntax after 11-nov-2019:
    -- https://github.com/FirebirdSQL/firebird/commit/a77295ba153e0c17061e2230d0ffdbaf08839114
    -- See also: doc/sql.extensions/README.set_bind.md:
    --     SET BIND OF type-from TO { type-to | LEGACY };
    --     SET BIND OF type NATIVE;

    set bind of decfloat to numeric(18,3);

    select cast('1234567.890' as DECFLOAT(34)) as test_01 from rdb$database;
    select cast('1234567.8901' as DECFLOAT(34)) as test_02 from rdb$database;
    select cast('12345678.901' as DECFLOAT(34)) as test_03 from rdb$database; -- 12345678.901
    select cast('12345678.90' as DECFLOAT(34)) as test_04 from rdb$database; -- Expected result is: 12345678.900
    select cast('9223372036854775.807' as DECFLOAT(34)) as test_05 from rdb$database;
"""

act = isql_act('db', test_script)

expected_stdout = """
    TEST_01                         1234567.890
    TEST_02                         1234567.890
    TEST_03                         12345678.901
    TEST_04                         12345678.900
    TEST_05                         9223372036854775.807
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
