#coding:utf-8

"""
ID:          issue-6339
ISSUE:       6339
TITLE:       BLOBs are unnecessarily copied during UPDATE after a table format change
DESCRIPTION:
  It's not easy to obtain BLOB_ID using only fdb. Rather in ISQL blob_id will be shown always (even if we do not want this :)).
  This test runs ISQL with commands that were provided in the ticket and parses its result by extracting only column BLOB_ID.
  Each BLOB_ID is added to set(), so eventually we can get total number of UNIQUE blob IDs that were generated during test.
  This number must be equal to number of records in the table (three in this test).
JIRA:        CORE-6089
FBTEST:      bugs.core_6089
"""

import pytest
import re
from firebird.qa import *

db = db_factory()

act = python_act('db')

test_script = """
    set bail on;
    set list on;
    set blob off;
    recreate table t (col1 int, col2 blob);
    recreate view v as select col2 as col2_blob_id from t; -- NB: alias for column have to be matched to re.compile() argument
    commit;

    insert into t values (1, '1');
    insert into t values (2, '2');
    insert into t values (3, '3');
    commit;

    select v.* from v;
    update t set col1 = -col1;
    select v.* from v;


    rollback;
    alter table t add col3 date;
    select v.* from v;
    update t set col1 = -col1;
    select v.* from v; -- bug was here
    quit;
"""

@pytest.mark.version('>=3.0.5')
def test_1(act: Action):
    pattern = re.compile('COL2_BLOB_ID\\s+\\S+', re.IGNORECASE)
    blob_id_set = set()
    act.isql(switches=['-q'], input=test_script)
    for line in act.stdout.splitlines():
        if pattern.search(line):
            blob_id_set.add(line.split()[1])
    # Check
    assert len(blob_id_set) == 3
