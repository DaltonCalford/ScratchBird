#coding:utf-8

"""
ID:          issue-2365
ISSUE:       2365
TITLE:       Possible AV in engine if procedure was altered to have no outputs and dependent procedures was not recompiled
DESCRIPTION:
JIRA:        CORE-1930
FBTEST:      bugs.core_1930
NOTES:
    [26.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214.
"""

import pytest
from firebird.qa import *

substitutions = [('Data source : Firebird::localhost:.*', 'Data source : Firebird::localhost:'), ('-At block line: [\\d]+, col: [\\d]+', '-At block line')]

db = db_factory()

# Statement that will be passed in ES/EDS (we have to check its presence in error message):
EDS_STATEMENT = 'create or alter procedure sp3 as begin  execute procedure sp2; end'

test_script = f"""
    set term ^;
    create or alter procedure sp1 returns (x int) as
    begin
      x=1;
      suspend;
    end
    ^

    create or alter procedure sp2 returns (x int) as
    begin
      select x from sp1 into :x;
      suspend;
    end
    ^

    create or alter procedure sp3 returns (x int)  as
    begin
      select x from sp2 into :x;
      suspend;
    end
    ^

    commit
    ^


    -- this is wrong but engine still didn't track procedure's fields dependencies
    create or alter procedure sp1
    as
    begin
      exit;
    end
    ^

    set term ;^
    commit;

    -- Here we create new attachment using specification of some non-null data in ROLE clause:
    set term ^;
    execute block as
    begin
        execute statement '{EDS_STATEMENT}'
        on external 'localhost:' || rdb$get_context('SYSTEM', 'DB_NAME')
        as user 'sysdba' password 'masterkey' role 'R1930';
    end
    ^
    set term ;^
    commit;
"""

act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = f"""
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -PARAMETER SP1.X
    -there are 1 dependencies

    Statement failed, SQLSTATE = 42000
    Execute statement error at isc_dsql_execute2 :
    335544351 : unsuccessful metadata update
    336397267 : CREATE OR ALTER PROCEDURE SP3 failed
    335544569 : Dynamic SQL Error
    335544850 : Output parameter mismatch for procedure SP2
    Statement : {EDS_STATEMENT}
    Data source : Firebird::localhost:
    -At block line
"""

expected_stdout_6x = f"""
    Statement failed, SQLSTATE = 42000
    unsuccessful metadata update
    -cannot delete
    -PARAMETER "PUBLIC"."SP1".X
    -there are 1 dependencies
    Statement failed, SQLSTATE = 42000
    Execute statement error at isc_dsql_execute2 :
    335544351 : unsuccessful metadata update
    336397267 : CREATE OR ALTER PROCEDURE "PUBLIC"."SP3" failed
    335544569 : Dynamic SQL Error
    335544850 : Output parameter mismatch for procedure "PUBLIC"."SP2"
    Statement : {EDS_STATEMENT}
    Data source : Firebird::localhost:
    -At block line
"""

@pytest.mark.es_eds
@pytest.mark.version('>=4.0')
def test_2(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
