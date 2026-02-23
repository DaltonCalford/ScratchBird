#coding:utf-8

"""
ID:          table.create-04
TITLE:       CREATE TABLE - constraints
DESCRIPTION:
FBTEST:      functional.table.create.04
NOTES:
    [12.07.2025] pzotov
    Removed 'SHOW' command.
    DML actions against a table must meet the DDL of such table.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.949; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set count on;
    recreate table test(
        id int
       ,pid int
       ,c1 smallint
       ,c2 smallint
       ,c3 smallint
       ,constraint test_pk primary key(id)
       ,constraint test_fk foreign key(pid) references test(id) on delete cascade
       ,constraint test_uk unique(c1)
       ,constraint test_ck check (c2 > c1)
    );
    insert into test(id) values(null); -- must fail
    insert into test(id, pid) values(1, null); -- must pass
    insert into test(id, pid) values(2, 1234); -- must pass
    update test set c1 = 1 where id = 1; -- must pass
    update test set c1 = 1 where id = 2; -- must fail
    update test set c2 = 1 where id = 1; -- must fail
    delete from test where id = 1; -- must pass and also must delete record with id = 2
    select * from test;
"""

substitutions = [('[ \t]+', ' '), ('(-)?At trigger .*', 'At trigger'), ('(-)?Problematic key value .*', 'Problematic key value')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    expected_stdout_5x = f"""
        Statement failed, SQLSTATE = 23000
        validation error for column "TEST"."ID", value "*** null ***"
        Records affected: 0
        Records affected: 1
        Statement failed, SQLSTATE = 23000
        violation of FOREIGN KEY constraint "TEST_FK" on table "TEST"
        -Foreign key reference target does not exist
        -Problematic key value is ("PID" = 1234)
        Records affected: 0
        Records affected: 1
        Records affected: 0
        Statement failed, SQLSTATE = 23000
        Operation violates CHECK constraint TEST_CK on view or table TEST
        -At trigger 'CHECK_3'
        Records affected: 0
        Records affected: 1
        Records affected: 0
    """
    expected_stdout_6x = f"""
        Statement failed, SQLSTATE = 23000
        validation error for column "PUBLIC"."TEST"."ID", value "*** null ***"
        Records affected: 0
        Records affected: 1
        Statement failed, SQLSTATE = 23000
        violation of FOREIGN KEY constraint "TEST_FK" on table "PUBLIC"."TEST"
        -Foreign key reference target does not exist
        -Problematic key value is ("PID" = 12341)
        Records affected: 0
        Records affected: 1
        Records affected: 0
        Statement failed, SQLSTATE = 23000
        Operation violates CHECK constraint "TEST_CK" on view or table "PUBLIC"."TEST"
        -At trigger "PUBLIC"."CHECK_3"
        Records affected: 0
        Records affected: 1
        Records affected: 0
    """
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
