#coding:utf-8

"""
ID:          issue-1353
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/1353
TITLE:       AV when blob is used in expression index
DESCRIPTION:
JIRA:        CORE-952
FBTEST:      bugs.core_0952
NOTES:
    [13.05.2023] pzotov
    Made SQL code be executed after DB initialization, i.e. using 'test_script' instead of init.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set bail on;
    set list on;
    recreate table test (
         id int primary key,
         b1 blob sub_type 1 segment size 80,
         c1 varchar(10)
    );
    commit;

    insert into test (id, b1, c1) values (1, 'www', 'qwer');
    commit;

    create index test_blob_substr_idx on test computed by (cast(substring(b1 from 1 for 100) as varchar(100)));
    commit;

    update test set test.c1 = 'asdf' where test.id = 1;
    commit; -- AV here
    select id, b1 as blob_id, c1 from test;
"""

act = isql_act('db', test_script, substitutions = [ ('BLOB_ID.*', ''), ] )

expected_stdout = """
    ID                              1
    BLOB_ID                         81:0
    www
    C1                              asdf
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
