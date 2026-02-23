#coding:utf-8

"""
ID:          issue-3539
ISSUE:       3539
TITLE:       Parameterized requests involving blob fields fails when connected using charset UTF8
DESCRIPTION:
JIRA:        CORE-3164
FBTEST:      bugs.core_3164
"""

import pytest
from firebird.qa import *

db = db_factory(charset='UTF8')

test_script = """
    create table test(fb blob);
    commit;
    insert into test values(rpad('', 7, 'foo') );
    insert into test values(rpad('', 8, 'bar') );
    commit;

    set list on;

    set term ^;
    execute block returns(n int) as
      declare v_blob blob;
    begin

      for
          select fb from test
          into v_blob
      do
          begin
            execute statement ('select octet_length(fb) from test where fb starting with ?') (:v_blob) into n;
            suspend;
          end
    end
    ^
    set term ;^
    -- Confirmed for 2.5.0 (WI-V2.5.0.26074):
    -- Statement failed, SQLSTATE = 22001
    -- arithmetic exception, numeric overflow, or string truncation
    -- -string right truncation
    -- (instead of output N = 8 for second record)
    -- Seems to be relatoed to core-3353 // Predicate (blob_field LIKE ?) describes the parameter as VARCHAR(30) rather than as BLOB
    -- because of SQLDA content:
    -- In 2.5.0: sqltype: 449 VARYING   Nullable sqlscale: 0 sqlsubtype: 4 sqllen: 28
    -- In 2.5.1: sqltype: 521 BLOB     Nullable sqlscale: 0 sqlsubtype: 0 sqllen: 8
"""

act = isql_act('db', test_script)

expected_stdout = """
    N                               7
    N                               8
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

