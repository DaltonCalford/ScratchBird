#coding:utf-8

"""
ID:          issue-6340
ISSUE:       6340
TITLE:       BLOB fields may be suddenly set to NULLs during UPDATE after a table format change
DESCRIPTION:
  It's not easy to obtain BLOB_ID using only fdb. Rather in ISQL blob_id will be shown always (even if we do not want this :)).
  This test runs ISQL with commands that were provided in the ticket and parses its result by extracting only column BLOB_ID.
  Each BLOB_ID is added to set(), so eventually we can get total number of UNIQUE blob IDs that were generated during test.
  This number must be equal to number of records in the table (three in this test).
  Beside of this, we check that all blobs are not null, see 'null_blob_cnt' counter.
JIRA:        CORE-6090
FBTEST:      bugs.core_6090
"""

import pytest
import re
from firebird.qa import *

db = db_factory()

act = python_act('db')

test_script = """
    set bail on;
    set blob all;
    set list on;

    recreate view v as select 1 x from rdb$database;
    commit;
    recreate table test (n1 int, n2 int, n3 int, blob_id blob);
    recreate view v as select blob_id from test;
    commit;

    insert into test values (0, 0, null, '0:foo');
    insert into test values (1, 1, 1,    '1:rio');
    insert into test values (2, 2, 2,    '2:bar');
    commit;

    select 1 as point, v.* from v;

    update test set n1 = 1 where n2 >= 0; -- n1 should be set to 1 in all three rows
    select 2 as point, v.* from v;
    rollback;

    update test set n1 = 1 where n2 >= 0 and n3 >= 0; -- n1 should be set to 1 in 2nd and 3rd rows
    select 3 as point, v.* from v;
    rollback;

    alter table test add col5 date;
    commit;

    update test set n1 = 1 where n2 >= 0; -- n1 should be set to 1 in all three rows
    select 4 as point, v.* from v; -- Here blob_id were changed because of other bug, see CORE-6089, but contents is correct
    rollback;

    update test set n1 = 1 where n2 >= 0 and n3 >= 0;
    -- n1 should be set to 1 in 2nd and 3rd rows
    select 5 as point, v.* from v; -- BUG: BLOB_ID in the second row was nullified!!!

    quit;
"""

@pytest.mark.version('>=3.0.5')
def test_1(act: Action):
    pattern = re.compile('BLOB_ID\\s+\\S+', re.IGNORECASE)
    blob_id_set = set()
    null_blob_cnt = 0
    act.isql(switches=['-q'], input=test_script)
    for line in act.stdout.splitlines():
        if pattern.search(line):
            blob_id_set.add(line.split()[1])
            if '<null>' in line.lower():
                null_blob_cnt += 1
    # Check
    assert len(blob_id_set) == 3
    assert null_blob_cnt == 0
