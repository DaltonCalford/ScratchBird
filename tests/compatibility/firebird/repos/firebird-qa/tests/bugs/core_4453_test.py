#coding:utf-8

"""
ID:          issue-4773
ISSUE:       4773
TITLE:       Regression: NOT NULL constraint, declared in domain, does not work
DESCRIPTION:
    Tests that manipulates with NULL fields/domains and check results:
    CORE-1518 Adding a non-null restricted column to a populated table renders the table inconsistent
    CORE-4453 (Regression: NOT NULL constraint, declared in domain, does not work)
    CORE-4725 (Inconsistencies with ALTER DOMAIN and ALTER TABLE with DROP NOT NULL and PRIMARY KEYs)
    CORE-4733 (Command "Alter table <T> alter TYPE <C> <DOMAIN_WITH_NOT_NULL" does not verifies data in column <C> and makes incorrect assignments in <C> to ZERO / JULIAN_DATE / ASCII(0) for types INT, TIMESTAMP and VARCHAR)
JIRA:        CORE-4453
FBTEST:      bugs.core_4453
    [29.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    create domain dm_01 varchar(20) not null;
    commit;
    create table t_01(s dm_01, x int);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    insert into t_01(x) values(100);
    select * from t_01 where s is null;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 23000
    validation error for column "T_01"."S", value "*** null ***"
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."T_01"."S", value "*** null ***"
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
