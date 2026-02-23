#coding:utf-8

"""
ID:          issue-1668
ISSUE:       1668
TITLE:       Server crash on select * from <recursive CTE>
DESCRIPTION: Simple select from recursive CTE crashes the server when query uses asterisk.
JIRA:        CORE-1244
FBTEST:      bugs.core_1244
"""

import pytest
from firebird.qa import *

init_script = """CREATE TABLE ARTICLES (ARTICLEID integer, PARENTID integer);
COMMIT;
INSERT INTO ARTICLES VALUES (1,NULL);
INSERT INTO ARTICLES VALUES (2,1);
COMMIT;"""

db = db_factory(init=init_script)

test_script = """with recursive
    Art_Tree as (
        select A.ArticleID
          from Articles A
         where A.ParentID is NULL

union all

select A.ArticleID
          from Articles A
               join Art_Tree T on (A.ParentID=T.ArticleID)
    )
select *
  from Art_Tree;
"""

act = isql_act('db', test_script)

expected_stdout = """ARTICLEID
============
           1
           2

"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

