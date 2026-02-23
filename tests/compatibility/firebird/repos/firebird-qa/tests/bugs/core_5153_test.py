#coding:utf-8

"""
ID:          issue-5436
ISSUE:       5436
TITLE:       Regression: Server crashes when aggregate functions are used together with NOT IN predicate
DESCRIPTION:
  Here we only check that server is alive after running query but WITHOUT checking its data
  (see: "and 1=0" below in HAVING section).
  We do NOT check correctness of query results - this will be done by another ticket, CORE-5165:
  wrong result in 3.0 RC2 and 4.0 comparing 2.5. It's NOT related to this ticket.
JIRA:        CORE-5153
FBTEST:      bugs.core_5153
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    select r.rdb$relation_id as id, count(*) as cnt
    from rdb$database r
    group by r.rdb$relation_id
    having count(*) not in (select r2.rdb$relation_id from rdb$database r2)
    and 1=0
    ;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.execute()

