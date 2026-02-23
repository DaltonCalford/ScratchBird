#coding:utf-8

"""
ID:          issue-5913
ISSUE:       5913
TITLE:       Increase number of formats/versions of views from 255 to 32K
DESCRIPTION:
JIRA:        CORE-5647
FBTEST:      bugs.core_5647
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    set term ^;
    execute block returns(ret_code smallint) as
        declare n int = 300;
    begin
        while (n > 0) do
          begin
            if (mod(n, 2) = 0) then
              begin
                in autonomous transaction do
                  begin
                    execute statement 'create or alter view vw1 (dump1) as select 1 from rdb$database';
                  end
              end
            else
              begin
                in autonomous transaction do
                  begin
                    execute statement 'create or alter view vw1 (dump1, dump2) as select 1, 2 from rdb$database';
                  end
              end
            n = n - 1;
          end
          ret_code = -abs(n);
          suspend;
    end ^
    set term ;^
    quit;
"""

act = isql_act('db', test_script)

expected_stdout = """
    RET_CODE                        0
"""

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
