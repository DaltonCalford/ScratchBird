#coding:utf-8

"""
ID:          isql-00
TITLE:       Check output of "HELP" and "HELP SET" commands
DESCRIPTION: NB: this test can also cover issue of CORE-2432 ("Missing SHOW COLLATIONs in HELP")
FBTEST:      functional.basic.isql.00
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    help;
    help set;
"""

act = isql_act('db', test_script, substitutions = [ ('[ \t]+', ' '), ])

fb3x_checked_stdout = """
    Frontend commands:
    BLOBDUMP <blobid> <file>   -- dump BLOB to a file
    BLOBVIEW <blobid>          -- view BLOB in text editor
    EDIT     [<filename>]      -- edit SQL script file and execute
    EDIT                       -- edit current command buffer and execute
    HELP                       -- display this menu
    INput    <filename>        -- take input from the named SQL file
    OUTput   [<filename>]      -- write output to named file
    OUTput                     -- return output to stdout
    SET      <option>          -- (Use HELP SET for complete list)
    SHELL    <command>         -- execute Operating System command in sub-shell
    SHOW     <object> [<name>] -- display system information
    <object> = CHECK, COLLATION, DATABASE, DOMAIN, EXCEPTION, FILTER, FUNCTION,
    GENERATOR, GRANT, INDEX, PACKAGE, PROCEDURE, ROLE, SQL DIALECT,
    SYSTEM, TABLE, TRIGGER, VERSION, USERS, VIEW
    EXIT                       -- exit and commit changes
    QUIT                       -- exit and roll back changes
    All commands may be abbreviated to letters in CAPitals

    Set commands:
    SET                    -- display current SET options
    SET AUTOddl            -- toggle autocommit of DDL statements
    SET BAIL               -- toggle bailing out on errors in non-interactive mode
    SET BLOB [ALL|<n>]     -- display BLOBS of subtype <n> or ALL
    SET BLOB               -- turn off BLOB display
    SET COUNT              -- toggle count of selected rows on/off
    SET MAXROWS [<n>]      -- limit select stmt to <n> rows, zero is no limit
    SET ECHO               -- toggle command echo on/off
    SET EXPLAIN            -- toggle display of query access plan in the explained form
    SET HEADING            -- toggle display of query column titles
    SET LIST               -- toggle column or table display format
    SET NAMES <csname>     -- set name of runtime character set
    SET PLAN               -- toggle display of query access plan
    SET PLANONLY           -- toggle display of query plan without executing
    SET SQL DIALECT <n>    -- set sql dialect to <n>
    SET STATs              -- toggle display of performance statistics
    SET TIME               -- toggle display of timestamp with DATE values
    SET TERM <string>      -- change statement terminator string
    SET WIDTH <col> [<n>]  -- set/unset print width to <n> for column <col>
    All commands may be abbreviated to letters in CAPitals
"""

fb5x_checked_stdout = """
    Frontend commands:
    BLOBDUMP <blobid> <file>   -- dump BLOB to a file
    BLOBVIEW <blobid>          -- view BLOB in text editor
    EDIT     [<filename>]      -- edit SQL script file and execute
    EDIT                       -- edit current command buffer and execute
    HELP                       -- display this menu
    INput    <filename>        -- take input from the named SQL file
    OUTput   [<filename>]      -- write output to named file
    OUTput                     -- return output to stdout
    SET      <option>          -- (Use HELP SET for complete list)
    SHELL    <command>         -- execute Operating System command in sub-shell
    SHOW     <object> [<name>] -- display system information
    <object> = CHECK, COLLATION, DATABASE, DOMAIN, EXCEPTION, FILTER, FUNCTION,
    GENERATOR, GRANT, INDEX, PACKAGE, PROCEDURE, ROLE, SQL DIALECT,
    SYSTEM, TABLE, TRIGGER, VERSION, USERS, VIEW, WIRE_STATISTICS
    EXIT                       -- exit and commit changes
    QUIT                       -- exit and roll back changes
    All commands may be abbreviated to letters in CAPitals
    Set commands:
    SET                    -- display current SET options
    SET AUTOddl            -- toggle autocommit of DDL statements
    SET BAIL               -- toggle bailing out on errors in non-interactive mode
    SET BLOB [ALL|<n>]     -- display BLOBS of subtype <n> or ALL
    SET BLOB               -- turn off BLOB display
    SET COUNT              -- toggle count of selected rows on/off
    SET MAXROWS [<n>]      -- limit select stmt to <n> rows, zero is no limit
    SET ECHO               -- toggle command echo on/off
    SET EXPLAIN            -- toggle display of query access plan in the explained form
    SET HEADING            -- toggle display of query column titles
    SET KEEP_TRAN_params   -- toggle to keep or not to keep text of following successful SET TRANSACTION statement
    SET LIST               -- toggle column or table display format
    SET NAMES <csname>     -- set name of runtime character set
    SET PER_TABle_stats    -- toggle display of detailed per-table statistics
    SET PLAN               -- toggle display of query access plan
    SET PLANONLY           -- toggle display of query plan without executing
    SET SQL DIALECT <n>    -- set sql dialect to <n>
    SET STATs              -- toggle display of performance statistics
    SET TIME               -- toggle display of timestamp with DATE values
    SET TERM <string>      -- change statement terminator string
    SET WIDTH <col> [<n>]  -- set/unset print width to <n> for column <col>
    SET WIRE_stats         -- toggle display of wire (network) statistics
    All commands may be abbreviated to letters in CAPitals
"""

fb6x_checked_stdout = """
    Frontend commands:
    BLOBDUMP <blobid> <file>   -- dump BLOB to a file
    BLOBVIEW <blobid>          -- view BLOB in text editor
    EDIT     [<filename>]      -- edit SQL script file and execute
    EDIT                       -- edit current command buffer and execute
    EXPLAIN                    -- explain a query access plan
    HELP                       -- display this menu
    INput    <filename>        -- take input from the named SQL file
    OUTput   [<filename>]      -- write output to named file
    OUTput                     -- return output to stdout
    SET      <option>          -- (Use HELP SET for complete list)
    SHELL    <command>         -- execute Operating System command in sub-shell
    SHOW     <object> [<name>] -- display system information
    <object> = CHECK, COLLATION, DATABASE, DOMAIN, EXCEPTION, FILTER, FUNCTION,
    GENERATOR, GRANT, INDEX, PACKAGE, PROCEDURE, ROLE, SQL DIALECT,
    SYSTEM, TABLE, TRIGGER, VERSION, USERS, VIEW, WIRE_STATISTICS
    EXIT                       -- exit and commit changes
    QUIT                       -- exit and roll back changes
    All commands may be abbreviated to letters in CAPitals
    Set commands:
    SET                    -- display current SET options
    SET AUTOddl            -- toggle autocommit of DDL statements
    SET AUTOTERM           -- toggle auto statement terminator
    SET BAIL               -- toggle bailing out on errors in non-interactive mode
    SET BLOB [ALL|<n>]     -- display BLOBS of subtype <n> or ALL
    SET BLOB               -- turn off BLOB display
    SET COUNT              -- toggle count of selected rows on/off
    SET MAXROWS [<n>]      -- limit select stmt to <n> rows, zero is no limit
    SET ECHO               -- toggle command echo on/off
    SET EXPLAIN            -- toggle display of query access plan in the explained form
    SET HEADING            -- toggle display of query column titles
    SET KEEP_TRAN_params   -- toggle to keep or not to keep text of following successful SET TRANSACTION statement
    SET LIST               -- toggle column or table display format
    SET NAMES <csname>     -- set name of runtime character set
    SET PER_TABle_stats    -- toggle display of detailed per-table statistics
    SET PLAN               -- toggle display of query access plan
    SET PLANONLY           -- toggle display of query plan without executing
    SET SQL DIALECT <n>    -- set sql dialect to <n>
    SET STATs              -- toggle display of performance statistics
    SET TIME               -- toggle display of timestamp with DATE values
    SET TERM <string>      -- change statement terminator string
    SET WIDTH <col> [<n>]  -- set/unset print width to <n> for column <col>
    SET WIRE_stats         -- toggle display of wire (network) statistics
    All commands may be abbreviated to letters in CAPitals
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = fb3x_checked_stdout if act.is_version('<5') else fb5x_checked_stdout if act.is_version('<6') else fb6x_checked_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
