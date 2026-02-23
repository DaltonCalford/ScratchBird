#coding:utf-8

"""
ID:          issue-5209
ISSUE:       5209
TITLE:       ALTER DOMAIN ... TO <new_name> allows to specify <new_name> matching to 'RDB$[[:DIGIT:]]*'
DESCRIPTION:
JIRA:        CORE-4917
FBTEST:      bugs.core_4917
NOTES:
    [30.06.2025] pzotov
    Added 'SQL_SCHEMA_PREFIX' and variables to store domain names - to be substituted in expected_* on FB 6.x
    Removed 'SHOW DOMAIN' command as its output has no matter for this test.
    Checked on 6.0.0.876; 5.0.3.1668; 4.0.6.3214; 3.0.13.33813.
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    -- CREATION:
    -- #########

    -- First, check that direct creation of domain with 'RDB$' prefix is disabled:
    create domain rdb$1 int;

    -- This also should FAIL becase new domain name is written in UPPER case (despite quotes):
    create domain "RDB$2" int;

    -- This should pass because new though name starts with 'rdb$' it
    -- is written in quotes and not in upper case:
    create domain "rdb$1" int;

    -- ALTERING:
    -- #########

    alter domain "rdb$1" to foo;

    alter domain foo to "rdb$1";

    -- This should pass because new though name starts with 'rdb$' it
    -- is written in quotes and not in upper case:
    alter domain "rdb$1" to "rdb$2";

    -- this should FAIL:
    alter domain "rdb$2" to RDB$3;

    -- this also should FAIL becase new domain name is written in UPPER case (despite quotes):
    alter domain "rdb$2" to "RDB$3";

"""

act = isql_act('db', test_script)

@pytest.mark.version('>=3.0')
def test_1(act: Action):

    SQL_SCHEMA_PREFIX = '' if act.is_version('<6') else '"PUBLIC".'
    DOMAIN_1_UPPER = 'RDB$1' if act.is_version('<6') else '"RDB$1"'
    DOMAIN_2_UPPER = 'RDB$2' if act.is_version('<6') else '"RDB$2"'
    DOMAIN_2_LOWER = 'rdb$2' if act.is_version('<6') else '"rdb$2"'

    expected_stdout = f"""
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE DOMAIN {SQL_SCHEMA_PREFIX}{DOMAIN_1_UPPER} failed
        -SQL error code = -637
        -Implicit domain name {SQL_SCHEMA_PREFIX}{DOMAIN_1_UPPER} not allowed in user created domain
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -CREATE DOMAIN {SQL_SCHEMA_PREFIX}{DOMAIN_2_UPPER} failed
        -SQL error code = -637
        -Implicit domain name {SQL_SCHEMA_PREFIX}{DOMAIN_2_UPPER} not allowed in user created domain
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER DOMAIN {SQL_SCHEMA_PREFIX}{DOMAIN_2_LOWER} failed
        -SQL error code = -637
        -Implicit domain name RDB$3 not allowed in user created domain
        Statement failed, SQLSTATE = 42000
        unsuccessful metadata update
        -ALTER DOMAIN {SQL_SCHEMA_PREFIX}{DOMAIN_2_LOWER} failed
        -SQL error code = -637
        -Implicit domain name RDB$3 not allowed in user created domain
    """

    act.expected_stdout = expected_stdout # _5x if act.is_version('<6') else expected_stdout_6x
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
