#coding:utf-8

"""
ID:          tabloid.loose-record-when-relief-where-expr
TITLE:       Uncomment of one part of OR'ed expression led to loose of record from resultset.
DESCRIPTION: 
  "OR" could __reduce__ number of rows in resultset.
      See leters to/from dimitr 23.09.2010 (probably this bug was due to occasional typo).
FBTEST:      functional.tabloid.loose_record_when_relief_where_expr
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
     set list on;
     with
     tset_init as
     (
         select
             61844284 srvt_id
             ,n.row_id
         from
         rdb$database lcat
         left join (
             select 1 row_id from rdb$database 
             union all select 2 from rdb$database
         ) n on 1=1
     )
     ,t_separate as
     (
         select
             srvt_id
             ,row_id
             ,v.c selected_case
         from tset_init
         join (
             select 'A' c from rdb$database 
             union all 
             select 'B' from rdb$database
         ) v
         on
         v.c='A'
         -- One record DISappeared from resultset when this part 
         -- of OR'ed expression was UNcommented.
         or v.c='B' and srvt_id=0 -- 2.5.0, /src/jrd/opt.cpp, line 4228: for (UCHAR i = 1; i <= streams[0]; i++)
     )
     select t.* from t_separate t
     ;
"""

act = isql_act('db', test_script)

expected_stdout = """
    SRVT_ID                         61844284
    ROW_ID                          1
    SELECTED_CASE                   A
    SRVT_ID                         61844284
    ROW_ID                          2
    SELECTED_CASE                   A
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
