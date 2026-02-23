#coding:utf-8

"""
ID:          table.alter-05
TITLE:       ALTER TABLE - ALTER - TO
DESCRIPTION:
FBTEST:      functional.table.alter.05
NOTES:
    [12.07.2025] pzotov
    Removed 'SHOW' command.
    Check that one can *not* rename column if some restrictions exist for that, see:
    https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref50/firebird-50-language-reference.html#fblangref50-ddl-tbl-altraltrto
    Statement 'ALTER TABLE...' followed by commit must allow further actions related to changed DDL.
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.949; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table test(
        wrong_named_id int
       ,wrong_named_pid int
       ,wrong_named_f01 int
       ,wrong_named_f02 int
       ,wrong_named_s01 varchar(20)
       ,wrong_named_s02 varchar(20)
       ,wrong_named_s03 varchar(20)
       ,constraint test_pk primary key(wrong_named_id)
       ,constraint test_fk foreign key (wrong_named_pid) references test(wrong_named_id)
       ,constraint test_uk unique(wrong_named_f01, wrong_named_f02)
       ,constraint test_ck check(lower(wrong_named_s02) is distinct from 'foo')
    );
    create view v_test as select distinct wrong_named_s03
    from test;

    set term ^;
    create trigger trg_test_biu for test active before insert or update as
    begin
        new.wrong_named_s01 = upper(new.wrong_named_s01);
    end ^
    set term ;^
    commit;
    ---------------------
    alter table test alter wrong_named_id to properly_named_id; -- must fail because column is PK
    alter table test alter wrong_named_pid to properly_named_pid; -- must fail because column is FK
    alter table test alter wrong_named_f02 to properly_named_f02; -- must fail because column is involved in UK
    alter table test alter wrong_named_s01 to properly_named_s01; -- must fail because column is mentioned in PSQL (trigger)
    alter table test alter wrong_named_s02 to properly_named_s02; -- must fail because column is mentioned in CHECK
    alter table test alter wrong_named_s03 to properly_named_s03; -- must fail because column presents in VIEW DDL
    drop view v_test;
    drop trigger trg_test_biu;
    alter table test
        drop constraint test_fk
       ,drop constraint test_uk
       ,drop constraint test_pk
       ,drop constraint test_ck
    ;

    -- now all must pass:
    alter table test
        alter wrong_named_id to properly_named_id
       ,alter wrong_named_pid to properly_named_pid
       ,alter wrong_named_f02 to properly_named_f02
       ,alter wrong_named_s01 to properly_named_s01
       ,alter wrong_named_s02 to properly_named_s02
       ,alter wrong_named_s03 to properly_named_s03
    ;
    commit;
    -- must pass
    set count on;
    insert into test(
         properly_named_id
        ,properly_named_pid
        ,properly_named_f02
        ,properly_named_s01
        ,properly_named_s02
        ,properly_named_s03
    ) values (
         1
        ,2
        ,3
        ,'qwe'
        ,'rty'
        ,'foo'
    );

"""

substitutions = [('[ \t]+', ' '), ('CHECK_\\d+','CHECK_x'), (r'cancelled by trigger \(\d+\)', 'cancelled by trigger')]
act = isql_act('db', test_script, substitutions = substitutions)

@pytest.mark.version('>=3')
def test_1(act: Action):

    expected_stdout_5x = f"""
        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TEST failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot update index segment used by an Integrity Constraint
        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TEST failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot update index segment used by an Integrity Constraint
        Statement failed, SQLSTATE = 27000
        unsuccessful metadata update
        -ALTER TABLE TEST failed
        -action cancelled by trigger (1) to preserve data integrity
        -Cannot update index segment used by an Integrity Constraint
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TEST failed
        -Column WRONG_NAMED_S01 from table TEST is referenced in TRG_TEST_BIU
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TEST failed
        -Column WRONG_NAMED_S02 from table TEST is referenced in CHECK_1
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE TEST failed
        -Column WRONG_NAMED_S03 from table TEST is referenced in V_TEST
        Records affected: 1
    """
    expected_stdout_6x = f"""
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE "PUBLIC"."TEST" failed
        -Cannot update index segment used by an Integrity Constraint
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE "PUBLIC"."TEST" failed
        -Cannot update index segment used by an Integrity Constraint
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE "PUBLIC"."TEST" failed
        -Cannot update index segment used by an Integrity Constraint
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE "PUBLIC"."TEST" failed
        -Column "WRONG_NAMED_S01" from table "PUBLIC"."TEST" is referenced in "PUBLIC"."TRG_TEST_BIU"
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE "PUBLIC"."TEST" failed
        -Column "WRONG_NAMED_S02" from table "PUBLIC"."TEST" is referenced in "PUBLIC"."CHECK_1"
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER TABLE "PUBLIC"."TEST" failed
        -Column "WRONG_NAMED_S03" from table "PUBLIC"."TEST" is referenced in "PUBLIC"."V_TEST"
        Records affected: 1
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
