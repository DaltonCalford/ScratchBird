#coding:utf-8

"""
ID:          issue-3669
ISSUE:       3669
TITLE:       Distinct aggregates return wrong (duplicated) data
DESCRIPTION:
  LIST() does not guarantee that returned values will be sorted so we can only count words
  in the resulting string and compare it with checked count.
JIRA:        CORE-3302
FBTEST:      bugs.core_3302
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table t( dt date);
    commit;
    insert into t values( '2011-01-07' );
    insert into t values( '2011-01-07' );
    insert into t values( '2011-01-07' );
    insert into t values( '2011-01-06' );
    insert into t values( '2011-01-06' );
    insert into t values( '2011-01-06' );
    insert into t values( '2011-01-08' );
    insert into t values( '2011-01-08' );
    insert into t values( '2011-01-08' );
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set list on;
    select char_length(s)-char_length(replace(s,',',''))+1 words_cnt, check_cnt
    from (
    select
       list(distinct
              case extract(weekday from dt)
                   when 0 then 'Sun' when 1 then 'Mon' when 2 then 'Tue'
                   when 3 then 'Wed' when 4 then 'Thu' when 5 then 'Fri'
                   when 6 then 'Sat' end
           ) s
       ,count(distinct dt) check_cnt
    from t
    )
    ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    WORDS_CNT                       3
    CHECK_CNT                       3
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

