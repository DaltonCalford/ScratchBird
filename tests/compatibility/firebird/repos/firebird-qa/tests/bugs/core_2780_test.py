#coding:utf-8

"""
ID:          issue-3171
ISSUE:       3171
TITLE:       Include client library version and protocol version in mon$attachments
DESCRIPTION:
JIRA:        CORE-2780
FBTEST:      bugs.core_2780
"""

import pytest
from firebird.qa import *

db = db_factory()

test_script = """
    set list on;
    -- See letter from dimitr, 10-apr-2015 09:19
    select
        -- All platform protovol name: starts with 'TCP'
        iif( cast(mon$remote_protocol as varchar(10) character set utf8) collate unicode_ci starting with 'tcp', 1, 0) is_protocol_valid
        -- Prefixes for client version:
        -- Windows: WI
        -- Linux:   LI
        -- MacOS:   UI (intel) or UP (powerpc)
        -- Solaris: SI (intel) or SO (spark)
        -- HP-UX:   HP
        -- '-T' = Testing; '-V' = RC or release
        -- Sufixes: 'Firebird' followed by space and at least one digit.
        ,iif( cast(mon$client_version as varchar(255) character set utf8) collate unicode_ci
              similar to
              '(WI|LI|UI|UP|SI|SO|HU)[-](T|V){0,1}[0-9]+.[0-9]+.[0-9]+((.?[0-9]+)*)([-]dev)?[[:WHITESPACE:]]+firebird[[:WHITESPACE:]]+[0-9]+((.?[0-9]+)*)%', 1, 0) is_client_version_valid
    from mon$attachments
    where mon$attachment_id = current_connection;
"""

act = isql_act('db', test_script)

expected_stdout = """
    IS_PROTOCOL_VALID               1
    IS_CLIENT_VERSION_VALID         1
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout

