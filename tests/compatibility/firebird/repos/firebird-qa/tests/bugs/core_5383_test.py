#coding:utf-8

"""
ID:          issue-5656
ISSUE:       5656
TITLE:       Dependencies in Package not recognised
DESCRIPTION:
JIRA:        CORE-5383
FBTEST:      bugs.core_5383
NOTES:
    [01.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.881; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    recreate package pg_03 as begin end;
    recreate package pg_02 as begin end;
    recreate package pg_01 as begin end;
    commit;

    recreate table test01 (id1 int primary key, f01 int);
    recreate table test02 (id2 int primary key, f02 int);
    recreate table test03 (id3 int primary key, f03 int);
    commit;

    insert into test01(id1, f01) values(1, 111);
    insert into test02(id2, f02) values(1, 222);
    insert into test03(id3, f03) values(1, 333);
    commit;

    set term ^;
    create package body pg_01 as
    begin
        procedure p01(i_x int) returns (o_y int) as
        begin
            --  dependencies is created for test01
            select id1 from test01 into :o_y;
        end
    end
    ^

    create package body pg_02 as
    begin
        procedure p01(i_x int) returns (o_y int)  as
        begin
            --  dependencies is created for test01 and test02 and test03
            select id1 from test01 into :o_y;
            select id2 from test02 into :o_y;
            select id3 from test03 into :o_y;
        end
    end
    ^

    recreate package pg_03 as
    begin
      procedure p01(i_x int) returns (o_y int);
      procedure p02(i_x int) returns (o_y int);
      procedure p03(i_x int) returns (o_y int);
    end
    ^

    create package body pg_03
    as
    begin
        procedure p01(i_x int) returns (o_y int) as
        begin
            -- no dependencies is created !!!
            select f01 from test01 where id1 = :i_x into :o_y;
        end

        procedure p02(i_x int) returns (o_y int) as
        begin
            -- no dependencies is created !!!
            select f02 from test02 where id2 = :i_x into :o_y;
        end

        procedure p03(i_x int) returns (o_y int) as
        begin
            --  dependencies is only created for test03
            select f03 from test03 where id3 = :i_x into :o_y;
        end
    end
    ^
    set term ;^
    commit;

    drop package pg_01;
    drop package pg_02;

    -- Following two drops PASSED WITHOUT ERROR on WI-V3.0.2.32620 and WI-T4.0.0.420 (BUG!).
    -- Expected STDERR:
    --   Statement failed, SQLSTATE = 42000
    --   unsuccessful metadata update
    --   -cannot delete
    --   -COLUMN TEST01.ID1 (TEST02,ID2)
    --   -there are 1 dependencies
    drop table test01;
    drop table test02;
    commit;

    set count on;
    select distinct rdb$dependent_name, rdb$depended_on_name from rdb$dependencies d;

    -- Following two statements issued on WI-V3.0.2.32620 and WI-T4.0.0.420:
    --   Statement failed, SQLSTATE = 2F000
    --   Error while parsing procedure PG_03.P01's (PG_03.P02's) BLR
    --   -table TEST01 (TEST02) is not defined
    -- Expected STDOUT:
    --   O_Y = 111 and O_Y = 222
    execute procedure pg_03.p01(1);
    execute procedure pg_03.p02(1);

    execute procedure pg_03.p03(1);
"""

substitutions = [] # [('[ \t]+', ' ')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN TEST01.ID1
    -there are 1 dependencies
    
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN TEST02.ID2
    -there are 1 dependencies
    
    RDB$DEPENDENT_NAME              PG_03
    RDB$DEPENDED_ON_NAME            TEST01
    RDB$DEPENDENT_NAME              PG_03
    RDB$DEPENDED_ON_NAME            TEST02
    RDB$DEPENDENT_NAME              PG_03
    RDB$DEPENDED_ON_NAME            TEST03
    Records affected: 3
    
    O_Y                             111
    O_Y                             222
    O_Y                             333
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN "PUBLIC"."TEST01"."ID1"
    -there are 1 dependencies

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -COLUMN "PUBLIC"."TEST02"."ID2"
    -there are 1 dependencies

    RDB$DEPENDENT_NAME              PG_03
    RDB$DEPENDED_ON_NAME            TEST01
    RDB$DEPENDENT_NAME              PG_03
    RDB$DEPENDED_ON_NAME            TEST02
    RDB$DEPENDENT_NAME              PG_03
    RDB$DEPENDED_ON_NAME            TEST03
    Records affected: 3

    O_Y                             111
    O_Y                             222
    O_Y                             333
"""

@pytest.mark.version('>=3.0.2')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
