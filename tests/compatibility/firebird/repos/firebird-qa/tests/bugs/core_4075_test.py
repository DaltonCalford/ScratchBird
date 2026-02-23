#coding:utf-8

"""
ID:          issue-4403
ISSUE:       4403
TITLE:       Server bugchecks or crashes on exception in calculated index
DESCRIPTION:
JIRA:        CORE-4075
FBTEST:      bugs.core_4075
NOTES:
    [18.10.2016]
    Added test case from #4918
    [16.09.2017]
    Added separate section for 4.0 because STDERR now contains name of index that causes problem (after core-5606 was implemented)
    [27.06.2025] pzotov
    Replaced subst: suppress any hyphen sign that occurs at starting position of every error message.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table TEST (BIT smallint);
    create index IDX_TEST_BIT on TEST computed by (bin_shl(1, TEST.BIT-1));

    -- from CORE-4603:
    recreate table T_TABLE (
        F_YEAR varchar(4),
        F_MONTH_DAY varchar(5)
    );
    create index T_INDEX on T_TABLE computed by (cast(F_YEAR || '.' || F_MONTH_DAY as date));
    commit;
    
    insert into test values (0);
    -- Trace:
    -- 335544606 : expression evaluation not supported
    -- 335544967 : Argument for BIN_SHL must be zero or positive
    -- from core-4603:
    insert into T_TABLE (F_YEAR, F_MONTH_DAY) values ('2014', '02.33');
"""

substitutions = [('^\\s*(-)?', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_3x = """
    Statement failed, SQLSTATE = 42000
    expression evaluation not supported
    Argument for BIN_SHL must be zero or positive

    Statement failed, SQLSTATE = 22018
    conversion error from string "2014.02.33"
"""

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    Expression evaluation error for index "IDX_TEST_BIT" on table "TEST"
    expression evaluation not supported
    Argument for BIN_SHL must be zero or positive

    Statement failed, SQLSTATE = 22018
    Expression evaluation error for index "T_INDEX" on table "T_TABLE"
    conversion error from string "2014.02.33"
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    Expression evaluation error for index "PUBLIC"."IDX_TEST_BIT" on table "PUBLIC"."TEST"
    expression evaluation not supported
    Argument for BIN_SHL must be zero or positive

    Statement failed, SQLSTATE = 22018
    Expression evaluation error for index "PUBLIC"."T_INDEX" on table "PUBLIC"."T_TABLE"
    conversion error from string "2014.02.33"
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
