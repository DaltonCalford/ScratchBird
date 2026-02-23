#coding:utf-8

"""
ID:          issue-6732
ISSUE:       6732
TITLE:       Stored procedure isn't able to execute statement 'GRANT'
DESCRIPTION:
  Confirmed bug on 4.0.0.2453, got:
    Statement failed, SQLSTATE = 27000
    unsuccessful metadata update
    -GRANT failed
    -action cancelled by trigger (0) to preserve data integrity
    -User cannot write to RDB$USER_PRIVILEGES
    -At procedure 'SP_TEST' line: 3, col: 8
NOTES:
[22.05.2021]
  This test initially had wrong value of min_version = 4.0
  Bug was fixed on 4.1.0.2468, build timestamp: 06-may-2021 12:34 thus min_version should be 4.1
  After several days this new FB branch was renamed to 5.0.
  Because of this, min_version for this test is 5.0
JIRA:        CORE-6502
FBTEST:      bugs.gh_6732
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create or alter user tmp$gh6732_boss password 'boss' using plugin Srp;
    create or alter user tmp$gh6732_acnt password 'acnt' using plugin Srp;
    commit;

    set term ^;
    create or alter procedure sp_test SQL SECURITY DEFINER as
    begin
       execute statement 'grant alter any generator to tmp$gh6732_acnt';
    end
    ^
    set term ;^
    commit;
    grant execute on procedure sp_test to user tmp$gh6732_boss;
    commit;

    connect '$(DSN)' user tmp$gh6732_boss password 'boss';
    execute procedure sp_test; -- must NOT raise error.
    commit;

    -- cleanup:
    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$gh6732_boss using plugin Srp;
    drop user tmp$gh6732_acnt using plugin Srp;
    commit;
"""

act = isql_act('db', test_script)

@pytest.mark.version('>=5.0')
def test_1(act: Action):
    act.execute()
