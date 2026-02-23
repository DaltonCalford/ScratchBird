#coding:utf-8

"""
ID:          issue-4938
ISSUE:       4938
TITLE:       Regression: SP "Domain" and "Type Of" based variables referring BLOB with
  sub_type < 0 no longer work
DESCRIPTION:
JIRA:        CORE-4623
FBTEST:      bugs.core_4623
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter procedure sp_test as begin end;
    recreate table test (id int);
    commit;
    set term ^;
    execute block as
    begin
        execute statement 'drop domain dm_01';
    when any do begin end
    end
    ^
    set term ;^
    commit;

    create domain dm_01 as blob sub_type -32768 segment size 32000;
    recreate table test (b_field dm_01);
    commit;

    set term ^;
    create or alter procedure sp_test (
        b01 blob sub_type -32768 segment size 32000,
        b02 type of column test.b_field,
        b03 dm_01
    ) as
    begin
    end
    ^
    create or alter trigger test_bi0 for test active before insert position 0 as
        declare b01 blob sub_type -32768 segment size 32000;
        declare b02 type of column test.b_field;
        declare b03 dm_01;
    begin
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
