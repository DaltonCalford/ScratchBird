#coding:utf-8

"""
ID:          issue-4535
ISSUE:       4535
TITLE:       Preserve comments for output parameters after altering procedures
DESCRIPTION:
JIRA:        CORE-4210
FBTEST:      bugs.core_4210
NOTES:
    [29.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' to be substituted in expected_* on FB 6.x
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;

    create or alter procedure sp_test as begin end;
    commit;
    recreate table test(id int);
    commit;
    set term ^;
    execute block as
    begin
      begin execute statement 'drop domain dm_int'; when any do begin end end
      begin execute statement 'drop domain dm_dts'; when any do begin end end
    end
    ^
    set term ;^
    commit;

    create domain dm_int int not null;
    create domain dm_dts timestamp;
    commit;

    recreate table test(id dm_int, dts dm_dts);
    commit;

    create or alter procedure sp_test (
         a_id1 dm_int,
         a_dts1 type of dm_dts
    ) returns (
         o_id1 type of column test.id,
         o_dts1 type of column test.dts
    ) as
    begin
    end
    ;
    comment on parameter sp_test.a_id1  is 'input id1';
    comment on parameter sp_test.a_dts1 is 'input timestamp1';
    comment on parameter sp_test.o_id1  is 'output id1';
    comment on parameter sp_test.o_dts1 is 'output timestamp1';
    commit;

    select '' "Before altering proc:" from rdb$database;
    show comments;

    alter procedure sp_test (
        a_id1 dm_int, ------------------- OLD name ==> old comment also has to be preserved
        a_dts2 type of column test.id
    ) returns (
        o_id1 type of column test.id, --- OLD name ==> old comment also has to be preserved
        o_dts2 type of column test.dts
    ) as
    begin
    end;
    commit;

    select '' "After altering proc:" from rdb$database;
    comment on parameter sp_test.a_dts2 is 'input timestamp2';
    comment on parameter sp_test.o_dts2 is 'output timestamp2';
    show comments;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else  'PUBLIC.'
    expected_stdout = f"""
        Before altering proc:

        COMMENT ON    PROCEDURE PARAMETER {SQL_SCHEMA_PREFIX}SP_TEST.A_ID1 IS input id1;
        COMMENT ON    PROCEDURE PARAMETER {SQL_SCHEMA_PREFIX}SP_TEST.A_DTS1 IS input timestamp1;
        COMMENT ON    PROCEDURE PARAMETER {SQL_SCHEMA_PREFIX}SP_TEST.O_ID1 IS output id1;
        COMMENT ON    PROCEDURE PARAMETER {SQL_SCHEMA_PREFIX}SP_TEST.O_DTS1 IS output timestamp1;

        After altering proc:

        COMMENT ON    PROCEDURE PARAMETER {SQL_SCHEMA_PREFIX}SP_TEST.A_ID1 IS input id1;
        COMMENT ON    PROCEDURE PARAMETER {SQL_SCHEMA_PREFIX}SP_TEST.A_DTS2 IS input timestamp2;
        COMMENT ON    PROCEDURE PARAMETER {SQL_SCHEMA_PREFIX}SP_TEST.O_ID1 IS output id1;
        COMMENT ON    PROCEDURE PARAMETER {SQL_SCHEMA_PREFIX}SP_TEST.O_DTS2 IS output timestamp2;
    """

    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

