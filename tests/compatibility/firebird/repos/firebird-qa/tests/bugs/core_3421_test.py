#coding:utf-8

"""
ID:          issue-3784
ISSUE:       3784
TITLE:       [FB3] AV with "UPDATE OR INSERT"
DESCRIPTION:
JIRA:        CORE-3421
FBTEST:      bugs.core_3421
NOTES:
    [11.12.2023] pzotov
    Added 'Error reading/writing' in substitutions: runtime error must not be filtered out by '?!(...)' pattern
    ("negative lookahead assertion", see https://docs.python.org/3/library/re.html#regular-expression-syntax).
    Added 'combine_output = True' in order to see message related to any error.

    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set sqlda_display on;

    recreate table test(
        id bigint constraint test_id_pk primary key using index test_id_pk,
        s01 varchar(512)
    );
    set term ^;
    create or alter procedure sp_test( a_id type of column test.id, a_s01 type of column test.s01)
    returns(o_id type of column test.id, o_s01 type of column test.s01) as
    begin
        execute statement ('update or insert into test(id, s01) values( :x, :y ) returning id, s01')
        ( x := a_id, y := a_s01 )
        into o_id, o_s01;
        suspend;
    end
    ^
    set term ;^
    commit;

    set planonly;
    set term ^;
    execute block returns(o_id type of column test.id, o_s01 type of column test.s01)
    as
    begin
        execute procedure sp_test(1, rpad('',512,'9876543210mnbvcxzasdfghjklpoiuytrewq')) returning_values o_id, o_s01;
        suspend;
    end
    ^
    set term ;^
    execute procedure sp_test(1, rpad('',512,'abcdefghjklmnopqrstuwwxyz012345678'));
    select * from sp_test(1, rpad('',512,'0123456789abcdefghjklmnopqrstuwwxyz'));
"""

# We have to use substitution which will not suppress "Error reading|writing data from the connection." message:
#
act = isql_act('db', test_script, substitutions = [ ('^((?!(sqltype|(Error\\s+(reading|writing)) )).)*$', ''), ('[ \t]+', ' ') ] )

expected_stdout_5x = """
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    02: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 512 charset: 0 NONE
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    02: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 512 charset: 0 NONE
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    02: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 512 charset: 0 NONE
"""

expected_stdout_6x = """
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    02: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 512 charset: 0 SYSTEM.NONE
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    02: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 512 charset: 0 SYSTEM.NONE
    01: sqltype: 580 INT64 Nullable scale: 0 subtype: 0 len: 8
    02: sqltype: 448 VARYING Nullable scale: 0 subtype: 0 len: 512 charset: 0 SYSTEM.NONE
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
