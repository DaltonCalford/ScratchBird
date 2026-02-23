#coding:utf-8

"""
ID:          issue-5665
ISSUE:       5665
TITLE:       BUGCHECK 179 (decompression overran buffer) or unexpected lock conflict may
  happen during record versions backout
DESCRIPTION:
  NOTE: bug can be reproduced only in SuperServer arch.

  We determine FB arch, and if it is SuperServer then change FW to OFF, add rows into table and
  perform statements that should raise internal FB CC.
  If no errors occures then ISQL log should contain number of affected rows.
  If internal FB CC will occur again then control will be returned to fbtest after ~2 minutes.

  For SS test lasts about 40 seconds, for SC/CS it should pass instantly because we SKIP entire test
  for both SC and CS architectures and just print 'OK' for matching expected_stdout.
JIRA:        CORE-5392
FBTEST:      bugs.core_5392
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db')

ROWS_CNT = 100000

expected_stdout = f"""
    Records affected: {ROWS_CNT}
"""

test_script = f"""
    create domain dm_longutf as varchar(8000) character set utf8;
    recreate table test (id int not null, a int);
    commit;

    set term ^;
    execute block as
      declare i int;
      declare n int = {ROWS_CNT}; -- (4.0 SS, page_size 8k: threshold is ~98000 records)
    begin
        while (n>0) do insert into test(id, a) values(:n, :n) returning :n-1 into n;
    end
    ^
    set term ;^
    commit;
    alter table test add constraint pk_test primary key (id) using descending index pk_test_desc;
    commit;

    alter table test add b dm_longutf default '' not null;
    commit;

    update test set a=2;
    rollback;

    set count on;
    -- Following UPDATE statement leads to:
    -- 1) on 3.0: decompression overran buffer (179), file: sqz.cpp line: 282
    -- 2) on 2.5.7.27030: decompression overran buffer (179), file: sqz.cpp line: 228
    -- Then FB waits (or is doing?) somewhat about 2 minutes abd after this
    -- control is returned to fbtest.
    update test set a=3;
    commit;
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    if act.get_server_architecture() == 'SuperServer':
        # Bucgcheck is reproduced on 2.5.7.27030 only when FW = OFF
        act.db.set_async_write() # This should be by default, but we just make sure it's OFF
        # Test
        act.expected_stdout = expected_stdout
        act.isql(switches=[], input=test_script)
        assert act.clean_stdout == act.clean_expected_stdout
