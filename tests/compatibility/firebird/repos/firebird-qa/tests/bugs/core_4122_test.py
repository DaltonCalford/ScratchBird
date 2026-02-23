#coding:utf-8

"""
ID:          issue-4450
ISSUE:       4450
TITLE:       Metadata export with isql (option -ex) does not export functions properly
DESCRIPTION:
JIRA:        CORE-4122
FBTEST:      bugs.core_4122
NOTES:
    [28.06.2025] pzotov
    Separated expected output for FB major versions prior/since 6.x.
    No substitutions are used to suppress schema and quotes. Discussed with dimitr, 24.06.2025 12:39.

    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

init_script = """
    set term ^ ;
    create or alter package PKG_TEST
    as
    begin
      function F_TEST_INSIDE_PKG
      returns smallint;
    end^
    set term ; ^

    set term ^ ;
    recreate package body PKG_TEST
    as
    begin
      function F_TEST_INSIDE_PKG
      returns smallint
      as
      begin
        return 1;
      end
    end^
    set term ; ^

    set term ^ ;
    create or alter function F_TEST_OUTSIDE_PKG
    returns smallint
    as
    begin
      return -1;
    end^

    set term ; ^
    commit;
"""

db = db_factory(init=init_script)

act = python_act('db', substitutions=[('/* CREATE DATABASE .*', '')])

expected_stdout_5x = """
    SET SQL DIALECT 3;

    /* CREATE DATABASE 'localhost/3330:test.fdb' PAGE_SIZE 4096 DEFAULT CHARACTER SET NONE; */


    COMMIT WORK;

    COMMIT WORK;
    SET AUTODDL OFF;
    SET TERM ^ ;

    /* Stored functions headers */
    CREATE OR ALTER FUNCTION F_TEST_OUTSIDE_PKG RETURNS SMALLINT
    AS
    BEGIN END ^

    SET TERM ; ^
    COMMIT WORK;
    SET AUTODDL ON;

    COMMIT WORK;
    SET AUTODDL OFF;
    SET TERM ^ ;

    /* Package headers */

    /* Package header: PKG_TEST, Owner: SYSDBA */
    CREATE PACKAGE PKG_TEST AS
    begin
      function F_TEST_INSIDE_PKG
      returns smallint;
    end^

    SET TERM ; ^
    COMMIT WORK;
    SET AUTODDL ON;

    COMMIT WORK;
    SET AUTODDL OFF;
    SET TERM ^ ;

    /* Stored functions bodies */

    ALTER FUNCTION F_TEST_OUTSIDE_PKG RETURNS SMALLINT
    AS
    begin
      return -1;
    end ^

    SET TERM ; ^
    COMMIT WORK;
    SET AUTODDL ON;

    COMMIT WORK;
    SET AUTODDL OFF;
    SET TERM ^ ;

    /* Package bodies */

    /* Package body: PKG_TEST, Owner: SYSDBA */

    CREATE PACKAGE BODY PKG_TEST AS
    begin
      function F_TEST_INSIDE_PKG
      returns smallint
      as
      begin
        return 1;
      end
    end^

    SET TERM ; ^
    COMMIT WORK;
    SET AUTODDL ON;
"""

expected_stdout_6x = """
    SET SQL DIALECT 3;
    /*
    /* Schema definitions */
    /* Schema: PUBLIC, Owner: SYSDBA */
    CREATE OR ALTER SCHEMA PUBLIC;
    COMMIT WORK;
    COMMIT WORK;
    COMMIT WORK;
    SET AUTODDL OFF;
    SET TERM ^ ;
    /* Stored functions headers */
    CREATE OR ALTER FUNCTION PUBLIC.F_TEST_OUTSIDE_PKG RETURNS SMALLINT
    AS
    BEGIN END ^
    SET TERM ; ^
    COMMIT WORK;
    SET AUTODDL ON;
    COMMIT WORK;
    SET AUTODDL OFF;
    SET TERM ^ ;
    /* Package headers */
    /* Package header: PUBLIC.PKG_TEST, Owner: SYSDBA */
    CREATE PACKAGE PUBLIC.PKG_TEST AS
    begin
    function F_TEST_INSIDE_PKG
    returns smallint;
    end^
    SET TERM ; ^
    COMMIT WORK;
    SET AUTODDL ON;
    COMMIT WORK;
    SET AUTODDL OFF;
    SET TERM ^ ;
    /* Stored functions bodies */
    ALTER FUNCTION PUBLIC.F_TEST_OUTSIDE_PKG RETURNS SMALLINT
    AS
    begin
    return -1;
    end ^
    SET TERM ; ^
    COMMIT WORK;
    SET AUTODDL ON;
    COMMIT WORK;
    SET AUTODDL OFF;
    SET TERM ^ ;
    /* Package bodies */
    /* Package body: PUBLIC.PKG_TEST, Owner: SYSDBA */
    CREATE PACKAGE BODY PUBLIC.PKG_TEST AS
    begin
    function F_TEST_INSIDE_PKG
    returns smallint
    as
    begin
    return 1;
    end
    end^
    SET TERM ; ^
    COMMIT WORK;
    SET AUTODDL ON;
    /* Grant permissions for this database */
    GRANT USAGE ON SCHEMA PUBLIC TO PUBLIC;
"""


@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.isql(switches = ['-x'], combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout


