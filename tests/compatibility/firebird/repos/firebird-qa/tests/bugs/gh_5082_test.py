#coding:utf-8

"""
ID:          issue-5082
ISSUE:       5082
TITLE:       Exception "too few key columns found for index" raises when attempt to create
  table with PK and immediatelly drop this PK within the same transaction [CORE4783]
DESCRIPTION:
JIRA:        CORE-4783
FBTEST:      bugs.gh_5082
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set autoddl off;
    commit;
    --set echo on;
    create table test(
         f01 varchar(2)
        ,constraint test_pk1 primary key (f01)
    );
    alter table test drop constraint test_pk1;

"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0.8')
def test_1(act: Action):
    act.execute()
