#coding:utf-8

"""
ID:          issue-5032
ISSUE:       5032
TITLE:       Inconsistencies with ALTER DOMAIN and ALTER TABLE with DROP NOT NULL and PRIMARY KEYs
DESCRIPTION:
JIRA:        CORE-4725
FBTEST:      bugs.core_4725
NOTES:
    [07.08.2024] pzotov
    Splitted expected* text because system triggers now are created in C++/GDML code
    See https://github.com/FirebirdSQL/firebird/pull/8202
    Commit (05-aug-2024 13:45):
    https://github.com/FirebirdSQL/firebird/commit/0cc8de396a3c2bbe13b161ecbfffa8055e7b4929

    [30.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- Tests that manipulates with NULL fields/domains and check results:
    -- CORE-1518 Adding a non-null restricted column to a populated table renders the table inconsistent
    -- CORE-4453 (Regression: NOT NULL constraint, declared in domain, does not work)
    -- CORE-4725 (Inconsistencies with ALTER DOMAIN and ALTER TABLE with DROP NOT NULL and PRIMARY KEYs)
    -- CORE-4733 (Command "Alter table <T> alter TYPE <C> <DOMAIN_WITH_NOT_NULL" does not verifies data in column <C> and makes incorrect assignments in <C> to ZERO / JULIAN_DATE / ASCII(0) for types INT, TIMESTAMP and VARCHAR)

    set list on;
    set heading off;

    recreate table test00(x integer);
    recreate table test01(x integer);
    recreate table test02(x integer);
    recreate table test03(x integer);
    recreate table test04(x integer);
    recreate table test05(x integer);
    recreate table test06(x integer);
    commit;

    set term ^;
    execute block as
    begin
        begin execute statement 'drop domain dm_01'; when any do begin end end
        begin execute statement 'drop domain dm_02'; when any do begin end end
        begin execute statement 'drop domain dm_03'; when any do begin end end
        begin execute statement 'drop domain dm_04'; when any do begin end end
        begin execute statement 'drop domain dm_05'; when any do begin end end
        begin execute statement 'drop domain dm_06'; when any do begin end end
    end
    ^
    set term ;^
    commit;

    create domain dm_01 integer;
    create domain dm_02 integer not null;
    create domain dm_03 integer not null;
    create domain dm_04 integer not null;
    create domain dm_05 integer;
    create domain dm_06 integer;
    commit;

    ------------------------------------------------------------------------------------------------------------

    recreate table test00(x integer);
    alter table test00 alter x set not null, add constraint t_pk primary key(x);
    commit;

    -- should produce "SQLSTATE = 27000 / Column used in a PRIMARY constraint must be NOT NULL.":
    alter table test00 alter x drop not null; -- correct error
    commit;

    select 'After try to drop NN on FIELD, NN was added by ALTER <C> SET NOT NULL' info_00
    from rdb$database;

    show table test00;

    ------------------------------------------------------------------------------------------------------------

    recreate table test01(x dm_01 not null);
    alter table test01 add constraint test01_pk primary key (x);
    commit;

    -- should produce "SQLSTATE = 27000 / Column used in a PRIMARY constraint must be NOT NULL.":
    alter table test01 alter x drop not null; -- correct error
    commit;

    select 'After try to drop NN on FIELD, NN was added directly in CREATE TABLE (<C> not null)' info_01
    from rdb$database;

    show table test01;


    ------------------------------------------------------------------------------------------------------------

    recreate table test02(x dm_02);
    alter table test02 add constraint test02_pk primary key (x);
    commit;

    -- ::: !!! :::
    -- This statement will NOT produce any error or warning but field will remain NOT null.
    -- Confirmed by ASF in core-4725 22.04.2015
    alter table test02 alter x drop not null; -- correct pass
    commit;

    select 'After try to drop NN on FIELD, NN was inherited from DOMAIN' info_02
    from rdb$database;

    show table test02;


    ------------------------------------------------------------------------------------------------------------

    recreate table test03(x dm_03);
    alter table test03 add constraint test03_pk primary key (x);
    commit;

    -- should produce "SQLSTATE = 42000 / Domain used in the PRIMARY KEY constraint of table TEST03 must be NOT NULL"
    alter domain dm_03 drop not null; -- incorrect pass !!!

    commit;

    select 'After try to drop NN on DOMAIN but dependent table exists' info_03
    from rdb$database;

    show domain dm_03;
    show table test03;

    ------------------------------------------------------------------------------------------------------------

    recreate table test04(x dm_04 not null);
    alter table test04 add constraint test04_pk primary key (x);
    commit;

    -- ::: !!! :::
    -- This statement will NOT produce any error or warning but field will remain NOT null.
    -- Confirmed by ASF in core-4725 22.04.2015
    alter table test04 alter x drop not null; -- ASF: "incorrect error !!!" // though it seems strange to me... :-/
    commit;

    select 'After try to drop NN on FIELD based on not-null domain, but NN was also specified in the field DDL' info_04
    from rdb$database;

    show table test04;

    ------------------------------------------------------------------------------------------------------------

    recreate table test05(x dm_05);
    commit;
    insert into test05 values(1);
    insert into test05 values(null);
    insert into test05 values(3);
    commit;

    -- Should produce "SQLSTATE = 22006 / Cannot make field X of table TEST05 NOT NULL because there are NULLs present"
    alter domain dm_05 set not null;
    commit;

    select 'After try to set NN on DOMAIN when at least one table exists with NULL in its data' info_05
    from rdb$database;
    show domain dm_05;


    ------------------------------------------------------------------------------------------------------------

    recreate table test06(x dm_06);
    commit;
    insert into test06 values(1);
    insert into test06 values(2);
    insert into test06 values(3);
    commit;

    -- Should pass:
    alter domain dm_06 set not null;
    commit;

    select 'After try to set NN on DOMAIN when NO table exists with NULL in its data' info_06
    from rdb$database;
    show domain dm_06;

    -- Should produce "SQLSTATE = 23000 / validation error for column "TEST06"."X", value "*** null ***""
    update test06 set x = null where x = 2;
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 27000
    unsuccessful metadata update
    -ALTER TABLE TEST00 failed
    -action cancelled by trigger (2) to preserve data integrity
    -Column used in a PRIMARY constraint must be NOT NULL.
    INFO_00                         After try to drop NN on FIELD, NN was added by ALTER <C> SET NOT NULL
    X                               INTEGER Not Null
    CONSTRAINT T_PK:
    Primary key (X)
    Statement failed, SQLSTATE = 27000
    unsuccessful metadata update
    -ALTER TABLE TEST01 failed
    -action cancelled by trigger (2) to preserve data integrity
    -Column used in a PRIMARY constraint must be NOT NULL.
    INFO_01                         After try to drop NN on FIELD, NN was added directly in CREATE TABLE (<C> not null)
    X                               (DM_01) INTEGER Not Null
    CONSTRAINT TEST01_PK:
    Primary key (X)
    INFO_02                         After try to drop NN on FIELD, NN was inherited from DOMAIN
    X                               (DM_02) INTEGER Not Null
    CONSTRAINT TEST02_PK:
    Primary key (X)
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER DOMAIN DM_03 failed
    -Domain used in the PRIMARY KEY constraint of table TEST03 must be NOT NULL
    INFO_03                         After try to drop NN on DOMAIN but dependent table exists
    DM_03                           INTEGER Not Null
    X                               (DM_03) INTEGER Not Null
    CONSTRAINT TEST03_PK:
    Primary key (X)
    INFO_04                         After try to drop NN on FIELD based on not-null domain, but NN was also specified in the field DDL
    X                               (DM_04) INTEGER Not Null
    CONSTRAINT TEST04_PK:
    Primary key (X)
    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field X of table TEST05 NOT NULL because there are NULLs present
    INFO_05                         After try to set NN on DOMAIN when at least one table exists with NULL in its data
    DM_05                           INTEGER Nullable
    INFO_06                         After try to set NN on DOMAIN when NO table exists with NULL in its data
    DM_06                           INTEGER Not Null
    Statement failed, SQLSTATE = 23000
    validation error for column "TEST06"."X", value "*** null ***"
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TABLE "PUBLIC"."TEST00" failed
    -Column used in a PRIMARY constraint must be NOT NULL.
    INFO_00                         After try to drop NN on FIELD, NN was added by ALTER <C> SET NOT NULL
    Table: PUBLIC.TEST00
    X                               INTEGER Not Null
    CONSTRAINT T_PK:
    Primary key (X)
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER TABLE "PUBLIC"."TEST01" failed
    -Column used in a PRIMARY constraint must be NOT NULL.
    INFO_01                         After try to drop NN on FIELD, NN was added directly in CREATE TABLE (<C> not null)
    Table: PUBLIC.TEST01
    X                               (PUBLIC.DM_01) INTEGER Not Null
    CONSTRAINT TEST01_PK:
    Primary key (X)
    INFO_02                         After try to drop NN on FIELD, NN was inherited from DOMAIN
    Table: PUBLIC.TEST02
    X                               (PUBLIC.DM_02) INTEGER Not Null
    CONSTRAINT TEST02_PK:
    Primary key (X)
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -ALTER DOMAIN "PUBLIC"."DM_03" failed
    -Domain used in the PRIMARY KEY constraint of table "PUBLIC"."TEST03" must be NOT NULL
    INFO_03                         After try to drop NN on DOMAIN but dependent table exists
    PUBLIC.DM_03                    INTEGER Not Null
    Table: PUBLIC.TEST03
    X                               (PUBLIC.DM_03) INTEGER Not Null
    CONSTRAINT TEST03_PK:
    Primary key (X)
    INFO_04                         After try to drop NN on FIELD based on not-null domain, but NN was also specified in the field DDL
    Table: PUBLIC.TEST04
    X                               (PUBLIC.DM_04) INTEGER Not Null
    CONSTRAINT TEST04_PK:
    Primary key (X)
    Statement failed, SQLSTATE = 22006
    unsuccessful metadata update
    -Cannot make field "X" of table "PUBLIC"."TEST05" NOT NULL because there are NULLs present
    INFO_05                         After try to set NN on DOMAIN when at least one table exists with NULL in its data
    PUBLIC.DM_05                    INTEGER Nullable
    INFO_06                         After try to set NN on DOMAIN when NO table exists with NULL in its data
    PUBLIC.DM_06                    INTEGER Not Null
    Statement failed, SQLSTATE = 23000
    validation error for column "PUBLIC"."TEST06"."X", value "*** null ***"
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
