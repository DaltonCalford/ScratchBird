#coding:utf-8

"""
ID:          issue-3410
ISSUE:       3410
TITLE:       Bugcheck "Too many savepoints (287)" at rollback after exception at EXECUTE BLOCK with exception handler
DESCRIPTION:
JIRA:        CORE-3029
FBTEST:      bugs.core_3029
NOTES:
    [27.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    create sequence test_gen;
    recreate table test_row
    (id int not null,
     did int not null,
     pid int not null,
     dep int not null
    );
    alter table test_row add constraint pk_test_row primary key(id);
    create unique index ix_test_row1 on test_row(did, pid, dep);
    commit;

    insert into test_row(id, did, pid,dep) values(1, 2, 3, 4);
    commit;

    set term ^;
    execute block returns(id int, did int, dep int, pid int)
    as
    declare variable xid int;
    begin
      select id,did, pid,dep
        from test_row
       where id=(select min(id) from test_row)
        into :xid, :did, pid, :dep;

      while (1=1) do
      begin
        delete from test_row r where r.id = :xid;

        insert into test_row(id, did, dep, pid)
        values (gen_id(test_gen, 1), :did, :dep, :pid);

        suspend;

      when any do
        exception;
      end
    end ^
    rollback ^
    create or alter procedure sp_test
      returns(id int, did int, dep int, pid int)
    as
    declare variable xid int;
    begin
      select id,did, pid,dep
        from test_row
       where id=(select min(id) from test_row)
        into :xid, :did, pid, :dep;

      while (1=1) do
      begin
        delete from test_row r where r.id = :xid;

        insert into test_row(id, did, dep, pid)
        values (gen_id(test_gen, 1), :did, :dep, :pid);

        suspend;

      when any do
        exception;
      end
    end ^
    select * from sp_test ^
    rollback ^
"""

substitutions = [ ('[ \t]+', ' '), ('line.*', ''), ('col.*', '')] 

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    ID <null>
    DID 2
    DEP 4
    PID 3
    ID <null>
    DID 2
    DEP 4
    PID 3
    Statement failed, SQLSTATE = 23000
    attempt to store duplicate value (visible to active transactions) in unique index "IX_TEST_ROW1"
    -Problematic key value is ("DID" = 2, "PID" = 3, "DEP" = 4)
    -At block
    -At block
    ID <null>
    DID 2
    DEP 4
    PID 3
    Statement failed, SQLSTATE = 23000
    attempt to store duplicate value (visible to active transactions) in unique index "IX_TEST_ROW1"
    -Problematic key value is ("DID" = 2, "PID" = 3, "DEP" = 4)
    -At procedure 'SP_TEST'
    -At procedure 'SP_TEST'
"""

expected_stdout_6x = """
    ID <null>
    DID 2
    DEP 4
    PID 3
    ID <null>
    DID 2
    DEP 4
    PID 3
    Statement failed, SQLSTATE = 23000
    attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."IX_TEST_ROW1"
    -Problematic key value is ("DID" = 2, "PID" = 3, "DEP" = 4)
    -At block
    -At block
    ID <null>
    DID 2
    DEP 4
    PID 3
    Statement failed, SQLSTATE = 23000
    attempt to store duplicate value (visible to active transactions) in unique index "PUBLIC"."IX_TEST_ROW1"
    -Problematic key value is ("DID" = 2, "PID" = 3, "DEP" = 4)
    -At procedure "PUBLIC"."SP_TEST"
    -At procedure "PUBLIC"."SP_TEST"
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
