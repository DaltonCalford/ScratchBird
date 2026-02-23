#coding:utf-8

"""
ID:          issue-2639
ISSUE:       2639
TITLE:       Offset value for SUBSTRING from BLOB more than 32767 makes exception
DESCRIPTION:
JIRA:        CORE-2211
FBTEST:      bugs.core_2211
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- [pcisar] 20.10.2021
    -- This script reports error:
    -- Statement failed, SQLSTATE = 54000
    -- arithmetic exception, numeric overflow, or string truncation
    -- -Implementation limit exceeded
    -- -At block line: 7, col: 9
    -- Statement failed, SQLSTATE = 22011
    -- Invalid offset parameter -1 to SUBSTRING. Only positive integers are allowed.

    recreate table test(b blob);
    commit;
    insert into test values('');
    commit;

    set list on;
    set blob all;

    set term ^;
    execute block as
      declare bsize int = 1000000;
      declare vclen int = 32760;
    begin
      while (bsize > 0) do
      begin
        update test set b  = b || rpad('', :vclen, uuid_to_char(gen_uuid()));
        bsize = bsize - vclen;
      end
      --update test set b = b || b;
      update test set b = b || rpad('', :vclen, '#');
    end
    ^
    set term ;^
    select char_length(b) from test;
    select substring(b from char_length(b)-1 for 1) from test;
    rollback;
"""

act = isql_act('db', test_script, substitutions=[('SUBSTRING.*', '')])

expected_stdout = """
    CHAR_LENGTH                     1048320
    SUBSTRING                       0:43
    #
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

