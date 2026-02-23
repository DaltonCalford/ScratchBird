#coding:utf-8

"""
ID:          issue-6145-A
ISSUE:       6145
TITLE:       Allow the use of management statements in PSQL blocks
DESCRIPTION:
  1. Currently test checks only ability to use SYNTAX for usage statements within PSQL blocks
     as it is described in '...\\doc\\sql.extensions\\README.management_statements_psql.md'.
     Actual affects on further work are not verified.
  2. Deferred check of 'alter session reset': got SQLSTATE = 25000/no transaction for request.
     Sent letter to dimitr et al, 09-mar-2019 13:43.
  3. Deferred check of 'SET TRUSTED ROLE': it is unclear for me how to implement this.
     Sent Q to Alex (same letter).

  All other cases are checked on 4.0.0.1421 and 4.0.0.1457: OK.
NOTES:
[10.12.2019]
  Updated syntax for SET BIND. Removed (maybe temply) check of 'set time zone bind native'.
  Commits that changed syntax:
  https://github.com/FirebirdSQL/firebird/commit/ca5ee672ca3144303732d5e2b5886b32c442da91 // 08-nov-19
  https://github.com/FirebirdSQL/firebird/commit/08096e3b879811d0e2e87b707b260c17d1542167 // 12-dec-19
JIRA:        CORE-5887
FBTEST:      bugs.core_5887
NOTES:
    [12.06.2022] pzotov
    Checked on 4.0.1.2692 - both on Linux and Windows.

    [25.11.2023] pzotov
    Writing code requires more care since 6.0.0.150: ISQL does not allow specifying duplicate delimiters without any statements between them (two semicolon, two carets etc).
"""

import pytest
from firebird.qa import *

db = db_factory()
tmp_role = role_factory('db', name='tmp_role_5887')
act = python_act('db')

@pytest.mark.version('>=4.0')
def test_1(act: Action, tmp_role: Role, capsys):

    test_script = f"""
        set bail on;

        grant {tmp_role.name} to sysdba;
        commit;

        ----------------------------------------------

        -- NO output should be produced by further PSQL blocks:

        set term ^;
        execute block as
        begin
            set decfloat round ceiling;
        end
        ^

        execute block as
        begin
            set decfloat traps to Division_by_zero, Invalid_operation, Overflow;
        end
        ^

        execute block as
        begin
            -- OLD SYNTAX: set decfloat bind native;
            -- Syntax after 11-nov-2019:
            -- https://github.com/FirebirdSQL/firebird/commit/a77295ba153e0c17061e2230d0ffdbaf08839114
            -- See also: doc/sql.extensions/README.set_bind.md:
            --     SET BIND OF type-from TO type-to | LEGACY ;
            --     SET BIND OF type NATIVE;

            set bind of decfloat to native;
            --                   ^^
            --                   +--- since 12-dec-2019
        end
        ^

        execute block as
        begin
            set role {tmp_role.name};
        end
        ^

        execute block as
        begin
            set session idle timeout 5 minute;
        end
        ^

        execute block as
        begin
            set statement timeout 1 minute;
        end
        ^

        execute block as
        begin
            set time zone local;
        end
        ^


        execute block as
        begin
            set bind of timestamp with time zone to legacy;
            set bind of time with time zone to legacy;
        end
        ^

        execute block as
        begin
            set bind of timestamp with time zone to native;
            set bind of time with time zone to native;
        end
        ^
        set term ;^
        commit;
    """

    act.isql(switches=['-q'], input=test_script)

    # NB: no sense in assert if expected output is empty.
