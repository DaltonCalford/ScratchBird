#coding:utf-8

"""
ID:          issue-8249
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/8249
TITLE:       CAST() ignores collation of target data type
NOTES:
    [22.10.2024] pzotov
    Commit related to this test (04.10.2024 13:13):
    https://github.com/FirebirdSQL/firebird/commit/aa167e2b36122684796d7b34935b0340be6f5074
    See also: gh_7748_test.py

    Confirmed problem on 6.0.0.483: queries to view, function, SP and EB complete OK (rather than expectedly raise error).
    Checked on 6.0.0.485 -- all OK. No output to STDOUT, all queries finish with errors.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create view v_test_1 as
    select cast('x' as varchar(10) character set utf8 collate missed_coll) as view_output from rdb$database
    ;
    commit;

    set term ^;
    create procedure sp_test_1 returns(sp_output varchar(10) character set utf8) as
    begin
        sp_output = cast('x' as varchar(10) character set utf8 collate missed_coll);
        suspend;
    end
    ^
    create procedure sp_test_2 returns(sp_output varchar(10) character set utf8) as
        declare v_text varchar(10) character set utf8;
    begin
        v_text = cast('x' as varchar(10) character set utf8 collate missed_coll);
        sp_output = v_text;
        suspend;
    end
    ^
    create function fn_test_1 returns varchar(10) character set utf8 as
    begin
        return cast('x' as varchar(10) character set utf8 collate missed_coll);
    end
    ^
    set term ;^
    commit;

    select * from v_test_1;
    select fn_test_1() as fn_output from rdb$database;
    select * from sp_test_1;

    set term ^;
    execute block returns(eb_text varchar(10) character set utf8) as
    begin
        execute procedure sp_test_2 returning_values :eb_text;
        suspend;
    end
    ^
    set term ;^

"""

act = isql_act('db', test_script, substitutions = [('[-]?At line \\d+.*', '')])

@pytest.mark.version('>=6.0')
def test_1(act: Action):

    expected_stdout_5x = """
        Statement failed, SQLSTATE = 22021
        unsuccessful metadata update
        -CREATE VIEW V_TEST_1 failed
        -Dynamic SQL Error
        -SQL error code = -204
        -COLLATION MISSED_COLL for CHARACTER SET UTF8 is not defined

        Statement failed, SQLSTATE = 22021
        unsuccessful metadata update
        -CREATE PROCEDURE SP_TEST_1 failed
        -Dynamic SQL Error
        -SQL error code = -204
        -COLLATION MISSED_COLL for CHARACTER SET UTF8 is not defined

        Statement failed, SQLSTATE = 22021
        unsuccessful metadata update
        -CREATE PROCEDURE SP_TEST_2 failed
        -Dynamic SQL Error
        -SQL error code = -204
        -COLLATION MISSED_COLL for CHARACTER SET UTF8 is not defined

        Statement failed, SQLSTATE = 22021
        unsuccessful metadata update
        -CREATE FUNCTION FN_TEST_1 failed
        -Dynamic SQL Error
        -SQL error code = -204
        -COLLATION MISSED_COLL for CHARACTER SET UTF8 is not defined

        Statement failed, SQLSTATE = 42S02
        Dynamic SQL Error
        -SQL error code = -204
        -Table unknown
        -V_TEST_1

        Statement failed, SQLSTATE = 39000
        Dynamic SQL Error
        -SQL error code = -804
        -Function unknown
        -FN_TEST_1

        Statement failed, SQLSTATE = 42S02
        Dynamic SQL Error
        -SQL error code = -204
        -Table unknown
        -SP_TEST_1

        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -204
        -Procedure unknown
        -SP_TEST_2
    """

    expected_stdout_6x = """
        Statement failed, SQLSTATE = 22021
        unsuccessful metadata update
        -CREATE VIEW "PUBLIC"."V_TEST_1" failed
        -Dynamic SQL Error
        -SQL error code = -204
        -COLLATION "PUBLIC"."MISSED_COLL" for CHARACTER SET "SYSTEM"."UTF8" is not defined
        Statement failed, SQLSTATE = 22021
        unsuccessful metadata update
        -CREATE PROCEDURE "PUBLIC"."SP_TEST_1" failed
        -Dynamic SQL Error
        -SQL error code = -204
        -COLLATION "PUBLIC"."MISSED_COLL" for CHARACTER SET "SYSTEM"."UTF8" is not defined
        Statement failed, SQLSTATE = 22021
        unsuccessful metadata update
        -CREATE PROCEDURE "PUBLIC"."SP_TEST_2" failed
        -Dynamic SQL Error
        -SQL error code = -204
        -COLLATION "PUBLIC"."MISSED_COLL" for CHARACTER SET "SYSTEM"."UTF8" is not defined
        Statement failed, SQLSTATE = 22021
        unsuccessful metadata update
        -CREATE FUNCTION "PUBLIC"."FN_TEST_1" failed
        -Dynamic SQL Error
        -SQL error code = -204
        -COLLATION "PUBLIC"."MISSED_COLL" for CHARACTER SET "SYSTEM"."UTF8" is not defined
        Statement failed, SQLSTATE = 42S02
        Dynamic SQL Error
        -SQL error code = -204
        -Table unknown
        -"V_TEST_1"
        Statement failed, SQLSTATE = 39000
        Dynamic SQL Error
        -SQL error code = -804
        -Function unknown
        -"FN_TEST_1"
        Statement failed, SQLSTATE = 42S02
        Dynamic SQL Error
        -SQL error code = -204
        -Table unknown
        -"SP_TEST_1"
        Statement failed, SQLSTATE = 42000
        Dynamic SQL Error
        -SQL error code = -204
        -Procedure unknown
        -"SP_TEST_2"
    """

    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
