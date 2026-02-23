#coding:utf-8

"""
ID:          issue-5196
ISSUE:       5196
TITLE:       Index corruption when add data in long-key-indexed field
DESCRIPTION:
    In order to check ticket issues this test does following:
    1. Change on test database FW to OFF - this will increase DML performance.
    2. Create table with indexed field of length = maximum that is allowed by
       current FB implementation (page_size / 4 - 9 bytes).
    3. Try to insert enough number of records in this table - this should cause
       runtime exception SQLSTATE = 54000, "Maximum index level reached"
    4. Start validation of database: index should NOT be corrupted in its report.
JIRA:        CORE-4904
FBTEST:      bugs.core_4904
NOTES:
    [30.06.2025] pzotov
    Test can not be executed on FB 6.x for suitable time because minimal page_size in this version is 8192.
    Number of rows that is needed to be inserted in index with key length = ~2K is too large.
"""

import pytest
from firebird.qa import *
from firebird.driver import DbWriteMode

substitutions = [('[0-9][0-9]:[0-9][0-9]:[0-9][0-9].[0-9][0-9]', ''),
                 ('Maximum index .* reached', 'Maximum index reached'),
                 ('Relation [0-9]{3,4}', 'Relation'), ('After line .*', ''),
                 ('-At block line: [\\d]+, col: [\\d]+', '-At block line')]

#  Test depends on 4K page_size !!!
db = db_factory(page_size=4096)

act = python_act('db', substitutions=substitutions)

long_keys_cmd = """
recreate table test(s varchar(1015)); -- with THIS length of field following EB will get exception very fast.
create index test_s on test(s);
commit;
set term ^;
execute block as
begin
  insert into test(s)
  select rpad('', 1015, uuid_to_char(gen_uuid()) )
  from rdb$types, rdb$types
  rows 50000; -- this is extra-huge reserve; exception should raise when about 120-130 rows will be inserted.
end
^
set term ;^
commit;
"""

expected_stdout = """
    Statement failed, SQLSTATE = 54000
    Implementation limit exceeded
    -Maximum index level reached
    -At block line: 3, col: 7

    Validation started
    Relation (TEST)
    process pointer page    0 of    1
    Index 1 (TEST_S)
    Relation (TEST) is ok
    Validation finished
"""

@pytest.mark.version('>=3.0,<6')
def test_1(act: Action, capsys):
    # Move database to FW = OFF in order to increase speed of insertions and output its header info:
    with act.connect_server() as srv:
        srv.database.set_write_mode(database=act.db.db_path, mode=DbWriteMode.ASYNC)
        # Preparing script for ISQL that will do inserts with long keys:
        act.expected_stderr = "We expect errors"
        act.isql(switches=[], input=long_keys_cmd)
        print(act.stdout)
        print(act.stderr)
        # Run validation after ISQL will finish (with runtime exception due to implementation limit exceeding):
        srv.database.validate(database=act.db.db_path, lock_timeout=1, callback=print)
        # Check
        act.expected_stdout = expected_stdout
        act.stdout = capsys.readouterr().out
        assert act.clean_stdout == act.clean_expected_stdout
