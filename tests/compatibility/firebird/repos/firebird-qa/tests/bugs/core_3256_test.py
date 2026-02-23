#coding:utf-8

"""
ID:          issue-3624
ISSUE:       3624
TITLE:       Error "request depth exceeded" may appear while preparing a select query
  against a view with explicit plan
DESCRIPTION:
JIRA:        CORE-3256
FBTEST:      bugs.core_3256
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter view vt(id,name) as select 1 id, '' name from rdb$database;
    commit;

    recreate table t1 (id integer not null,name integer not null);
    alter table t1 add constraint pk_t1 primary key (id)
    using index pk_t1
    ;
    create or alter view vt(id,name) as select * from t1;
    commit;

    -- repeat ~130 times
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    ---
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));

    -- The following statement leads to:
    -- Statement failed, SQLSTATE = 54001
    -- unsuccessful metadata update
    -- -request depth exceeded. (Recursive definition?)
    -- Confirmed for 2.5.0 only, 26.02.2015.
    --#####################################################
    select * from vt where id=1 PLAN (vt T1 INDEX (PK_T1));
    --#####################################################
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.execute()
