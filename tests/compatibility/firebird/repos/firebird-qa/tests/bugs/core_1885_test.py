#coding:utf-8

"""
ID:          issue-897
ISSUE:       897
TITLE:       CREATE COLLATION connection lost under Posix
DESCRIPTION: Issuing 'CREATE COLLATION' forced FB to crash on Posix when using LOCALE option
NOTES:
[15.1.2022] pcisar
  For 3.0.7 on Linux this PASS (uses system ICU) but on Windows (includes ICU 52)
  it FAIL unless newer ICU (63) is installed. Hence as this issue was POSIX-only,
  we'll not run it on Windows.

[15.04.2022] pzotov
  About FB 3.x: this test always raises exception on Windows but, in contrary, always passes on POSIX.
  Originally this test was created only to ensure that FB does not crash, but outcome (pass/fail) not matters.
  It was decided to make this test act the same on both operating systems, and the simplest way for that is
  do run CREATE COLLATION within EXCEUTE BLOCK and suppress any exception that can occurs there
  See discussion with Alex, 15.04.2022 17:58.

JIRA:        CORE-1885
FBTEST:      bugs.core_1885
"""

import pytest
from firebird.qa import *

db = db_factory()

# version: 3.0
##############
test_script_1 = """
    -- Following EB:
    -- must complete normally (w/o any exception) on LINUX;
    -- must raise exception on Windows but it was decided to SUPPRESS such exception if its gdscode = 335544351 and sqlstate =  'HY000'
    -- (any other kind of exception will not be suppressed and we will able to see its details).
    set list on;
    set term ^;
    execute block returns(outcome_msg varchar(100), raised_gds int, raised_sqlstate char(5)) as
    begin
        execute statement q'{CREATE COLLATION UNICODE_ENUS_CI_3X FOR UTF8 FROM UNICODE CASE INSENSITIVE 'LOCALE=en_US'}';
        when any do
        begin
            raised_gds = gdscode;
            raised_sqlstate = sqlstate;
            if ( not( raised_gds = 335544351 and raised_sqlstate = 'HY000' ) ) then
            begin
                outcome_msg = 'Unexpected gdscode and/or sqlstate:';
                suspend;
            end
        end
    end
    ^
    set term ^;
    commit;
"""

act_1 = isql_act('db', test_script_1)

expected_stdout_1 = """
"""


@pytest.mark.version('>=3.0,<4.0')
def test_1(act_1: Action):
    act_1.expected_stdout = expected_stdout_1
    act_1.execute()
    assert act_1.clean_stdout == act_1.clean_expected_stdout


# version: 4.0
##############

subst_fb4x = [    
                ('SPECIFIC_ATTR_BLOB_ID.*', 'SPECIFIC_ATTR_BLOB_ID')
               ,('COLL-VERSION=\\d+.\\d+(;ICU-VERSION=\\d+.\\d+)?.*', 'COLL-VERSION=<attr>')
             ]

test_script_2 = """
    -- ::: NB ::: 31.01.2019
    -- Since builds 4.0.0.1410, 26.01.2019, FULL ICU library icudt63l.dat is included in snapshot,
    -- so this collation *CAN AND MUST* be created w/o errors.

    set list on;
    set count on;
    create collation unicode_enus_ci_4x for utf8 from unicode case insensitive 'LOCALE=en_US';
    commit;

    select
        rc.rdb$collation_name
        ,rc.rdb$collation_attributes
        ,rc.rdb$base_collation_name
        ,rc.rdb$specific_attributes as specific_attr_blob_id
        ,rs.rdb$character_set_name
        --,rs.rdb$number_of_characters
        ,rs.rdb$bytes_per_character
    from rdb$collations rc
    join rdb$character_sets rs
    on rc.rdb$character_set_id = rs.rdb$character_set_id
    where
        rc.rdb$system_flag is distinct from 1
        and rc.rdb$collation_name = upper('unicode_enus_ci_4x');
"""

act_2 = isql_act('db', test_script_2, substitutions = subst_fb4x)

expected_stdout_2 = """
    RDB$COLLATION_NAME              UNICODE_ENUS_CI_4X

    RDB$COLLATION_ATTRIBUTES        3
    RDB$BASE_COLLATION_NAME         UNICODE

    SPECIFIC_ATTR_BLOB_ID           1d:1e7
    COLL-VERSION=<attr>
    RDB$CHARACTER_SET_NAME          UTF8

    RDB$BYTES_PER_CHARACTER         4

    Records affected: 1
"""

@pytest.mark.version('>=4.0')
def test_2(act_2: Action):
    act_2.expected_stdout = expected_stdout_2
    act_2.execute()
    assert act_2.clean_stdout == act_2.clean_expected_stdout

