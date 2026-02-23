#coding:utf-8

"""
ID:          issue-1692
ISSUE:       1692
TITLE:       Ceation of invalid procedures/triggers allowed
DESCRIPTION:
JIRA:        CORE-1271
FBTEST:      bugs.core_1271
NOTES:
    [25.06.2025] pzotov
        Separated expected output for FB major versions prior/since 6.x.
        No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
        Minimal snapshot number for 6.x: 6.0.0.863, see letter from Adriano, 24.06.2025 23:24, commit:
        https://github.com/FirebirdSQL/firebird/commit/79ff650e5af7a0d6141e166b0cb8208ef211f0a7
        Checked on 6.0.0.863; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
    [13.02.2026] pzotov
        Adjusted output for 5.x and 6.x to actual (changed after #14c6de6e "Improvement #8895...").
        Confirmed by Vlad, 13.02.2026 1006.
        Checked on 6.0.0.1454, 5.0.4.1762, 4.0.7.3245, 3.0.14.33838.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create table test(id int primary key using index test_id, f01 timestamp);
    create index test_f01 on test(f01);
    commit;
    set term ^;
    create procedure sp_test (a_id int) returns (o_f01 type of column test.f01) as
    begin
        for 
            select f01 from test where id = :a_id plan (test order test_f01) 
            into o_f01
        do
            suspend;
    end
    ^
    commit
    ^
"""

substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_4x = """
    Statement failed, SQLSTATE = 2F000
    Error while parsing procedure SP_TEST's BLR
    -index TEST_F01 cannot be used in the specified plan
"""

expected_stdout_5x = """
    Statement failed, SQLSTATE = 2F000
    index TEST_F01 cannot be used in the specified plan
    -Error while parsing procedure SP_TEST's BLR
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 2F000
    index "PUBLIC"."TEST_F01" cannot be used in the specified plan
    -Error while parsing procedure "PUBLIC"."SP_TEST"'s BLR
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_4x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout

