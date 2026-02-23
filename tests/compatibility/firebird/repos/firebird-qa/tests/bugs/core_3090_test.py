#coding:utf-8

"""
ID:          issue-3469
ISSUE:       3469
TITLE:       Incorrect LEFT JOIN result using table and derived constant subquery
DESCRIPTION:
JIRA:        CORE-3090
FBTEST:      bugs.core_3090
"""

import pytest
from firebird.qa import *

init_script = """recreate table test_err (id int);
insert into test_err (ID) values (1);
insert into test_err (ID) values (2);
commit;
"""

db = db_factory(init=init_script)

test_script = """select *
  from test_err t
       left join (select 1 id from rdb$database) a on a.id = t.id;
select
  RESULT
from (select
    'Well' as RESULT
  from (select
      (select 'Does not work' from RDB$DATABASE) as D
    from RDB$DATABASE d) i
  left join RDB$DATABASE on 1=0) j
left join RDB$DATABASE on 1=0;
"""

act = isql_act('db', test_script)

expected_stdout = """
          ID           ID
============ ============
           1            1
           2       <null>


RESULT
======
Well

"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

