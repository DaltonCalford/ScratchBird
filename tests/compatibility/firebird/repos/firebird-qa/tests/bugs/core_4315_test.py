#coding:utf-8

"""
ID:          issue-4638
ISSUE:       4638
TITLE:       Usage of field(s) alias in view WITH CHECK OPTION leads to incorrect compile error or incorrect internal triggers
DESCRIPTION:
JIRA:        CORE-4315
FBTEST:      bugs.core_4315
NOTES:
    [15.07.2025] pzotov
    Confirmed bug on 3.0.0.30830-9c050ab (13-jan-2014), got:
        SQLSTATE = 42S22 / ... / -RECREATE VIEW v_test failed / ... / -Column unknown / -T1.N2
    Last issue from ticket ("Compile but generates incorrect internal triggers") can not be checked.
    Test verifies only first three examples from ticket.
    Checked on 3.0.0.30834-fc6110d - result is expected

    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.970; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    recreate table t1 (n1 integer, n2 integer);
    recreate view v_test as select t1.n1 from t1 as t1 where t1.n1 < t1.n2 with check option;

    insert into t1(n1, n2) values(1, 3);
    update v_test set n1 = n1 + 1;
    update v_test set n1 = n1 + 1; -- must fail
    select * from t1;
    rollback;

    recreate view v_test as select t1.n1 from t1 where t1.n1 < t1.n2 with check option;
    insert into t1(n1, n2) values(1, 4);
    update v_test set n1 = n1 * 2;
    update v_test set n1 = n1 * 2; -- must fail
    select * from t1;
    rollback;

    recreate view v_test as select x.n1 from t1 as x where x.n1 < x.n2 with check option;
    insert into t1(n1, n2) values(1, 5);
    update v_test set n1 = n1 * 3;
    update v_test set n1 = n1 * 3; -- must fail
    select * from t1;
    rollback;
"""

substitutions = [('[ \t]+', ' '), ('CHECK_\\d+', 'CHECK_x')]
act = isql_act('db', test_script, substitutions = substitutions)


@pytest.mark.version('>=3.0')
def test_1(act: Action):

    expected_stdout_5x = """
        Statement failed, SQLSTATE = 23000
        Operation violates CHECK constraint  on view or table V_TEST
        -At trigger 'CHECK_1'

        N1                              2
        N2                              3

        Statement failed, SQLSTATE = 23000
        Operation violates CHECK constraint  on view or table V_TEST
        -At trigger 'CHECK_3'

        N1                              2
        N2                              4

        Statement failed, SQLSTATE = 23000
        Operation violates CHECK constraint  on view or table V_TEST
        -At trigger 'CHECK_5111'

        N1                              3
        N2                              5
    """

    expected_stdout_6x = """
        Statement failed, SQLSTATE = 23000
        Operation violates CHECK constraint  on view or table "PUBLIC"."V_TEST"
        -At trigger "PUBLIC"."CHECK_1"

        N1                              2
        N2                              3

        Statement failed, SQLSTATE = 23000
        Operation violates CHECK constraint  on view or table "PUBLIC"."V_TEST"
        -At trigger "PUBLIC"."CHECK_3"

        N1                              2
        N2                              4

        Statement failed, SQLSTATE = 23000
        Operation violates CHECK constraint  on view or table "PUBLIC"."V_TEST"
        -At trigger "PUBLIC"."CHECK_5111"

        N1                              3
        N2                              5
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x

    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
