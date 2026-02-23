#coding:utf-8

"""
ID:          issue-8710
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8710
TITLE:       Regression in FB 6.x for plan in legacy form: excessive comma (",") between items of <schema.index> list
NOTES:
    [21.08.2025] pzotov
    Checked on 6.0.0.1232-770890c
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create table test(a varchar(10) primary key using index test_a, b varchar(10));
    create index test_b on test(b);

    set planonly;
    select 1 from test x
    where
        exists(
            select 1 from
            test y
            where
                y.b is null or
                y.b = x.b
                and y.a < x.a
        );
"""
substitutions = [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout = f"""
        PLAN ("Y" INDEX ("PUBLIC"."TEST_B", "PUBLIC"."TEST_B", "PUBLIC"."TEST_A"))
        PLAN ("X" NATURAL)
    """
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
