#coding:utf-8

"""
ID:          issue-6483
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/6483
TITLE:       Procedures and EXECUTE BLOCK without RETURNS should not be allowed to use SUSPEND
DESCRIPTION:
JIRA:        CORE-6239
FBTEST:      bugs.core_6239
NOTES:
    [05.02.2020] pzotov
    Fix was done by commit https://github.com/FirebirdSQL/firebird/commit/b2b5f9a87cea26a9f12fa231804dba9d0426d3fa
    (can be checked by 4.0.0.1763+, date of build since 05-feb-2020).

    [02.07.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.
    Checked on 6.0.0.889; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- these two collations and domain not needed for TEST per se.
    -- We create them here only for reference in declaration of SP input params, see below:
    create collation nums_coll for utf8 from unicode case insensitive 'NUMERIC-SORT=1';
    create collation name_coll for utf8 from unicode no pad case insensitive accent insensitive;
    create domain dm_test varchar(20) character set utf8 default 'foo' not null collate nums_coll;
    commit;

    -- All following statements should be declined with
    --     Statement failed, SQLSTATE = 42000
    --     Dynamic SQL Error
    --     -SQL error code = -104
    --     -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK

    set term ^ ;
    execute block as
    begin
       suspend;
    end
    ^

    execute block (a_x int not null = ?) as
    begin
       suspend;
    end
    ^

    create or alter procedure sp_missed_returns_in_its_header_1 as
    begin
       -- we use SUSPEND clause but there is no
       -- output parameters in this SP header:
       suspend;
    end
    ^

    create or alter procedure sp_missed_returns_in_its_header_2 (a_x varchar(10) not null) as
    begin
       suspend;
    end
    ^

    create or alter procedure sp_missed_returns_in_its_header_3 (p1 varchar(20) character set utf8 not null collate nums_coll default 'foo') SQL SECURITY DEFINER as
    begin
       suspend;
    end
    ^


    recreate package pg_test_1 as
    begin
        procedure pg_proc(
            p1 varchar(20) character set utf8 not null collate nums_coll default 'foo'
           ,p2 dm_test default 'qwe'
           ,p3 dm_test default 'bar'
           ,p4 dm_test collate name_coll default 'rio'
        );
    end
    ^

    create package body pg_test_1 as
    begin
        procedure pg_proc(
            p1 varchar(20) character set utf8 not null collate nums_coll
           ,p2 dm_test
           ,p3 dm_test
           ,p4 dm_test collate name_coll
        ) as
        begin
            suspend;
        end
    end
    ^
    set term ;^
"""

act = isql_act('db', test_script)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK

    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PROCEDURE SP_MISSED_RETURNS_IN_ITS_HEADER_1 failed
    -Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PROCEDURE SP_MISSED_RETURNS_IN_ITS_HEADER_2 failed
    -Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PROCEDURE SP_MISSED_RETURNS_IN_ITS_HEADER_3 failed
    -Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK

    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE PACKAGE BODY PG_TEST_1 failed
    -Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PROCEDURE "PUBLIC"."SP_MISSED_RETURNS_IN_ITS_HEADER_1" failed
    -Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PROCEDURE "PUBLIC"."SP_MISSED_RETURNS_IN_ITS_HEADER_2" failed
    -Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE OR ALTER PROCEDURE "PUBLIC"."SP_MISSED_RETURNS_IN_ITS_HEADER_3" failed
    -Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -CREATE PACKAGE BODY "PUBLIC"."PG_TEST_1" failed
    -Dynamic SQL Error
    -SQL error code = -104
    -SUSPEND could not be used without RETURNS clause in PROCEDURE or EXECUTE BLOCK
"""

@pytest.mark.version('>=4.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
