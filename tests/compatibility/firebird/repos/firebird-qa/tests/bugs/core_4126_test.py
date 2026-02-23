#coding:utf-8

"""
ID:          issue-4454
ISSUE:       4454
TITLE:       gbak -r fails in restoring all stored procedures/functions in packages
DESCRIPTION:
  Test creates table and procedure + function - both standalone and packaged.
  Then we do (with saving result in logs):
  1) checking query;
  2) isql -x
  After this we try to backup and restore - STDERR should be empty.
  Finally, we try again to run checking query and extract metadata - and compare
  their result with previously stored one.
  Difference between them should be EMPTY with excluding name of databases.
JIRA:        CORE-4126
FBTEST:      bugs.core_4126
"""

import pytest
from io import BytesIO
from difflib import unified_diff
from firebird.qa import *
from firebird.driver import SrvRestoreFlag

init_script = """
    create or alter procedure p01 as begin end;
    create or alter function f01() returns int as begin end;
    recreate package pg_01 as begin end;
    commit;

    recreate table test (x smallint);
    commit;

    set term ^;
    execute block as
    begin
        begin
          execute statement 'drop domain dm_nums';
          when any do begin end
        end
        begin
          execute statement 'drop collation nums_coll';
          when any do begin end
        end
    end
    ^
    set term ;^
    commit;

    create collation nums_coll for utf8 from unicode case insensitive 'NUMERIC-SORT=1';
    create domain dm_nums as varchar(10) character set utf8 collate nums_coll;
    commit;

    recreate table test (id int, s dm_nums);
    commit;

    set term ^;
    create or alter procedure p01(a_id int) returns (o_s type of dm_nums ) as
    begin
            for select reverse(s) from test where id = :a_id into o_s do suspend;
    end
    ^
    create or alter function f01(a_id int) returns dm_nums as
    begin
            return reverse((select s from test where id = :a_id));
    end
    ^
    recreate package pg_01 as
    begin
        procedure p01(a_id int) returns (o_s type of dm_nums );
        function f01(a_id int) returns dm_nums;
    end
    ^
    create package body pg_01 as
    begin
        procedure p01(a_id int) returns (o_s type of dm_nums ) as
        begin
            for select s from test where id = :a_id into o_s do suspend;
        end
        function f01(a_id int) returns dm_nums as
        begin
            return (select s from test where id = :a_id);
        end
    end
    ^
    set term ;^
    commit;

    insert into test(id, s) values(1, '1234');
    insert into test(id, s) values(2, '125');
    insert into test(id, s) values(3, '16');
    commit;

"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('CREATE DATABASE.*', 'CREATE DATABASE'),
                                        ('CREATE COLLATION .*', 'CREATE COLLATION')])

dml_test = """
set list on;

select t.id, p.o_s as standalone_proc_result
from test t left join p01(t.id) p on 1=1
order by 2;

select t.id, f01(t.id) as standalone_func_result from test t
order by 2;

select t.id, p.o_s as packaged_proc_result
from test t left join pg_01.p01(t.id) p on 1=1
order by 2;

select t.id, pg_01.f01(t.id) as packaged_func_result from test t
order by 2;
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    # gather metadta and test cript result before backup & restore
    act.isql(switches=['-x'])
    meta_before = act.stdout
    act.reset()
    act.isql(switches=[], input=dml_test)
    dml_before = act.stdout
    #
    backup = BytesIO()
    with act.connect_server() as srv:
        srv.database.local_backup(database=act.db.db_path, backup_stream=backup)
        backup.seek(0)
        srv.database.local_restore(database=act.db.db_path, backup_stream=backup,
                                   flags=SrvRestoreFlag.REPLACE)
    # gather metadta and test cript result after backup & restore
    act.reset()
    act.isql(switches=['-x'])
    meta_after = act.stdout
    act.reset()
    act.isql(switches=[], input=dml_test)
    dml_after = act.stdout
    # check
    assert list(unified_diff(meta_before, meta_after)) == []
    assert list(unified_diff(dml_before, dml_after)) == []
