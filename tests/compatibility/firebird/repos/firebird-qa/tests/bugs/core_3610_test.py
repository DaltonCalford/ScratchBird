#coding:utf-8

"""
ID:          issue-3964
ISSUE:       3964
TITLE:       Can insert DUPLICATE keys in UNIQUE index
DESCRIPTION:
JIRA:        CORE-3610
FBTEST:      bugs.core_3610
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
    recreate table test(id int not null, f01 int, constraint test_unq unique(f01) using index test_unq);
    commit;
    insert into test values(1, 1 );
    insert into test values(2,null);
    insert into test values(3,null);
    commit;
    set transaction read committed record_version no wait;
    update test set f01=null where id=1;
    set term ^;
    execute block as
    begin
        execute statement ('update test set f01 = ? where id = ?') (1, 3)
        with autonomous transaction
        on external ( 'localhost:'||rdb$get_context('SYSTEM','DB_NAME') )
        as user 'sysdba' password 'masterkey' role 'role_02'
        ;
    end
    ^
    set term ;^
    rollback;

    set list on;
    select * from test;
"""

substitutions = [('[ \t]+', ' '),
                 ('Data source : Firebird::localhost:.*', 'Data source : Firebird::localhost:'),
                 ('335544382 : Problematic key', '335545072 : Problematic key'),
                 ('-At block line: [\\d]+, col: [\\d]+', '-At block line')]
act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout_5x = """
    Statement failed, SQLSTATE = 42000
    Execute statement error at isc_dsql_execute2 :
    335544665 : violation of PRIMARY or UNIQUE KEY constraint "TEST_UNQ" on table "TEST"
    335545072 : Problematic key value is ("F01" = 1)
    Statement : update test set f01 = ? where id = ?
    Data source : Firebird::localhost:
    -At block line
    ID                              1
    F01                             1
    ID                              2
    F01                             <null>
    ID                              3
    F01                             <null>
"""

expected_stdout_6x = """
    Statement failed, SQLSTATE = 42000
    Execute statement error at isc_dsql_execute2 :
    335544665 : violation of PRIMARY or UNIQUE KEY constraint "TEST_UNQ" on table "PUBLIC"."TEST"
    335545072 : Problematic key value is ("F01" = 1)
    Statement : update test set f01 = ? where id = ?
    Data source : Firebird::localhost:
    -At block line
    ID                              1
    F01                             1
    ID                              2
    F01                             <null>
    ID                              3
    F01                             <null>
"""

@pytest.mark.es_eds
@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
