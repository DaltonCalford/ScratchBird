#coding:utf-8

"""
ID:          issue-4001
ISSUE:       4001
TITLE:       recreation of collation for utf8 from unicode with option NUMERIC-SORT=1 leads to FB death
DESCRIPTION:
JIRA:        CORE-3650
FBTEST:      bugs.core_3650
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    recreate table tns(f int); -- drop dependencies if any
    commit;
    set term ^;
    execute block as
    begin
      execute statement 'drop collation ns_coll;';
      when any do begin end
    end^
    set term ;^
    commit;
    create collation ns_coll for utf8 from unicode 'NUMERIC-SORT=1';
    recreate table tns(s varchar(50) character set utf8 collate ns_coll);
    commit;

    recreate table tns(f int); -- drop dependencies if any
    commit;
    set term ^;
    execute block as
    begin
      execute statement 'drop collation ns_coll;';
      when any do begin end
    end^
    set term ;^
    commit;
    create collation ns_coll for utf8 from unicode 'NUMERIC-SORT=1';
    rollback; -- !!NB!!

    set term ^;
    execute block as
    begin
      execute statement 'drop collation ns_coll;';
      when any do begin end
    end^
    set term ;^
    commit; -- this commit kills FB service
    show collation;
"""

act = isql_act('db', test_script)

expected_stderr = """
  There are no user-defined collations in this database
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stderr = expected_stderr
    act.execute()
    assert act.clean_stderr == act.clean_expected_stderr

