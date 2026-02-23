#coding:utf-8

"""
ID:          issue-3609
ISSUE:       3609
TITLE:       Makes GEN_UUID return a compliant RFC-4122 UUID
DESCRIPTION:
JIRA:        CORE-3238
FBTEST:      bugs.core_3238
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    execute block returns(err_cnt int) as
      declare n int = 100000;
      declare s varchar(36);
    begin
      err_cnt = 0;
      while( n > 0 ) do
      begin
        s = uuid_to_char( gen_uuid() );
        if ( NOT (substring(s from 15 for 1)='4' and substring(s from 20 for 1) in ('8','9','A','B')) )
          then err_cnt = err_cnt + 1;
        n = n - 1;
      end
      suspend;
    end
    ^ set term ;^
"""

act = isql_act('db', test_script)

expected_stdout = """
    ERR_CNT                         0
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

