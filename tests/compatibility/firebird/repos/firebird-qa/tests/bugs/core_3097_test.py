#coding:utf-8

"""
ID:          issue-3476
ISSUE:       3476
TITLE:       Updating blob field cause server crash with ACCESS_VIOLATION exception
DESCRIPTION:
JIRA:        CORE-3097
FBTEST:      bugs.core_3097
"""

import pytest
from zipfile import Path
from firebird.qa import *

db = db_factory()

act = isql_act('db', '')

expected_stdout = """
    ID                              32765
    CHAR_LENGTH                     32765
    OCTET_LENGTH                    32765

    ID                              32766
    CHAR_LENGTH                     32766
    OCTET_LENGTH                    32766

    ID                              32767
    CHAR_LENGTH                     32767
    OCTET_LENGTH                    32767

    ID                              32768
    CHAR_LENGTH                     32768
    OCTET_LENGTH                    32768

    ID                              32769
    CHAR_LENGTH                     32769
    OCTET_LENGTH                    32769

    ID                              65532
    CHAR_LENGTH                     65532
    OCTET_LENGTH                    65532

    ID                              65533
    CHAR_LENGTH                     65533
    OCTET_LENGTH                    65533

    ID                              65534
    CHAR_LENGTH                     65534
    OCTET_LENGTH                    65534

    ID                              65535
    CHAR_LENGTH                     65535
    OCTET_LENGTH                    65535
"""

expected_stderr = """
    Statement failed, SQLSTATE = 42000
    Dynamic SQL Error
    -SQL error code = -104
    -String literal with 65536 bytes exceeds the maximum length of 65535 bytes
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    script_file = Path(act.files_dir / 'core_3097.zip',
                    at='core_3097_script.sql')
    act.script = script_file.read_text()
    act.expected_stdout = expected_stdout
    act.expected_stderr = expected_stderr
    act.execute()
    assert (act.clean_stderr == act.clean_expected_stderr and
            act.clean_stdout == act.clean_expected_stdout)

