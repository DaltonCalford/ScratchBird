#coding:utf-8

"""
ID:          issue-875
ISSUE:       875
TITLE:       Permissions are checked case-insensitively
DESCRIPTION:
JIRA:        CORE-521
FBTEST:      bugs.core_0521
    [23.06.2025] pzotov
    Expected output was separated depending on FB version: we have to show SCHEMA name as prefix for DB object (since 6.0.0.834).
    Checked on 6.0.0.853; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    create user tmp$c0521 password '123';
    commit;

    set term ^;
    create procedure perm
    as begin
    end^

    create procedure "PeRm"
    as begin
      execute procedure perm;
    end^

    create procedure "pErM"
    as begin
      --execute procedure perm;
      execute procedure "PeRm";
    end^
    set term ;^
    commit;

    grant execute on procedure perm to procedure "PeRm";
    grant execute on procedure "pErM" to user tmp$c0521;
    commit;

    connect '$(DSN)' user tmp$c0521 password '123';
    set list on;
    select current_user as whoami from rdb$database;
    execute procedure "pErM";

    commit;
    connect '$(DSN)' user sysdba password 'masterkey';
    drop user tmp$c0521;
    commit;
"""

substitutions = [('[ \t]+', ' '), ('execute', 'EXECUTE'), ('-Effective user is.*', '')]
act = isql_act('db', test_script, substitutions = substitutions)

expected_stdout_5x = """
    WHOAMI TMP$C0521
    Statement failed, SQLSTATE = 28000
    no permission for EXECUTE access to PROCEDURE PeRm
"""

expected_stdout_6x = """
    WHOAMI TMP$C0521
    Statement failed, SQLSTATE = 28000
    no permission for EXECUTE access to PROCEDURE "PUBLIC"."PeRm"
"""

@pytest.mark.version('>=3')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout


