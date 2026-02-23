#coding:utf-8

"""
ID:          issue-6934
ISSUE:       6934
TITLE:       SQL SECURITY DEFINER does not affect the ownership of created DDL objects
DESCRIPTION:
  Test invokes procedure that is declared with 'SQL SECURITY DEFINER' two times:
  1) for executing 'CREATE TABLE' statement WITHOUT specifying privileges (and current user in this case is 'SENIOR');
  2) for executing 'CREATE TABLE' *with* specifyin 'WITH CALLER PRIVILEGES' clause (and current user is 'JUNIOR').

  Then it queries to RDB$ tables in order to get info about ONWER of created tables.
  In any case owner must be 'SYSDBA' rather than 'SENIOR' or 'JUNIOR'.

  NB. Clause 'WITH CALLER PRIVILEGE' must *not* affect on result, owner will be 'SYSDBA' (as it is w/o such clause).
  This is because that clause replaces access rights of current user with rights of STORED PROCEDURE.
  Before 4.x this meaned access rights that were explicitly granted to this SP.
  But since 4.x SECURITY DEFINER set access rights of SP equal to its owner (i.e. SYSDBA).
  It means that WITH CALLER PRIVILEGES does the same, i.e. it is pointless in this case.
  Explained by dimitr, see letter 08-sep-2021 10:06.
FBTEST:      bugs.gh_6934
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    set term ^;
    create procedure sp_test( a_do_with_caller_access smallint default 0 ) sql security definer as
    begin
      if ( a_do_with_caller_access <> 1 ) then
          execute statement 'recreate table table_for_senior(id int)';
      else
          execute statement 'recreate table table_for_junior(id int)'
          with caller privileges
          ;
    end
    ^
    set term ;^
    commit;

    create or alter user tmp$gh_6934_senior password '456';
    create or alter user tmp$gh_6934_junior password '123';

    grant create table to user tmp$gh_6934_senior;
    grant execute on procedure sp_test to tmp$gh_6934_senior;
    grant execute on procedure sp_test to tmp$gh_6934_junior;
    commit;

    connect '$(DSN)' user tmp$gh_6934_senior password '456';
    execute procedure sp_test(0);

    commit;
    connect '$(DSN)' user tmp$gh_6934_junior password '123';
    execute procedure sp_test(1);
    commit;

    set count on;

    select rdb$relation_name, rdb$owner_name
    from rdb$relations
    where rdb$relation_name in ( upper('table_for_senior'), upper('table_for_junior') )
    order by rdb$relation_name
    ;

    set blobdisplay 3;

    select rdb$acl as rdb_acl_blob_id
    from rdb$relations
    join rdb$security_classes using (rdb$security_class)
    where rdb$relation_name  in ( upper('table_for_senior'), upper('table_for_junior') );
    commit;

    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$gh_6934_senior;
    drop user tmp$gh_6934_junior;
    commit;
"""

act = isql_act('db', test_script, substitutions=[('RDB_ACL_BLOB_ID.*', ''), ('[ \t]+', ' '),
                                                 ('ACL version.*', '')])

expected_stdout = """
    RDB$RELATION_NAME               TABLE_FOR_JUNIOR
    RDB$OWNER_NAME                  SYSDBA

    RDB$RELATION_NAME               TABLE_FOR_SENIOR
    RDB$OWNER_NAME                  SYSDBA

    Records affected: 2

    RDB_ACL_BLOB_ID                 9:d89
    ACL version 1
    person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)
    RDB_ACL_BLOB_ID                 9:d75
    ACL version 1
    person: SYSDBA, privileges: (alter, control, drop, insert, update, delete, select, references)

    Records affected: 2
"""

@pytest.mark.version('>=4.0.1')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
