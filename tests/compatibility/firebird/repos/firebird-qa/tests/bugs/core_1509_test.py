#coding:utf-8

"""
ID:          issue-1924
ISSUE:       1924
TITLE:       Conversion from double to varchar insert trailing spaces
DESCRIPTION:
JIRA:        CORE-1509
FBTEST:      bugs.core_1509
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
     set list on;
     select
          iif( position(' z' in t1)>0, 'BAD: >' || t1 || '<', 'OK.') as chk1
         ,iif( position(' z' in t2)>0, 'BAD: >' || t2 || '<', 'OK.') as chk2
         ,iif( position(' z' in t3)>0, 'BAD: >' || t3 || '<', 'OK.') as chk3
     from (
         select
              cast(exp(-744.0346068132731393)-exp(-745.1332191019410399) as varchar(32))||'z' t1
             ,cast(sin(0) as varchar(32))||'z' t2
             ,cast(cast(0e0 as double precision) as varchar(32))||'z' t3
         from rdb$database
     );
"""

act = isql_act('db', test_script)

expected_stdout = """
    CHK1                            OK.
    CHK2                            OK.
    CHK3                            OK.
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

