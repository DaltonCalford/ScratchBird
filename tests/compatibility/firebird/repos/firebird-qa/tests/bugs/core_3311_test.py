#coding:utf-8

"""
ID:          issue-3678
ISSUE:       3678
TITLE:       Error "data type unknown" while preparing UPDATE/DELETE statements with the parameterized ROWS clause
DESCRIPTION:
JIRA:        CORE-3311
FBTEST:      bugs.core_3311
NOTES:
    [27.06.2025] pzotov
    Reimplemented: it is enough to check only STDERR in this test rather that compare issued execution plans.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    recreate table test(id int);
    commit;
    insert into test select rand()*1000 from rdb$types,(select 1 i from rdb$types rows 10);
    commit;
    create index test_id on test(id);
    commit;
"""

db = db_factory(init=init_script)

test_script = """
    set planonly;
    select * from test rows ?;
    select * from test where id between ? and ? order by id  rows ? to ?;
    update test set id=id rows ? to ?;
    update test set id=id where id between ? and ? order by id rows ? to ?;
    delete from test rows ? to ?;
    delete from test where id between ? and ? order by id  rows ? to ?;
    merge into test t
    using(
      select id from test where id between ? and ? rows ?
    ) s
    on t.id=s.id
    when matched then update set t.id=s.id;
    merge into test t
    using(
      select id from test where id between ? and ? order by id rows ?
    ) s
    on t.id=s.id
    when matched then update set t.id=s.id;
    set planonly;
"""

act = isql_act('db', test_script)

expected_stderr = """
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute() # ::: do not use 'combine_outpt = True! We have to check here only STDERR :::
    assert act.clean_stderr == act.clean_expected_stderr

