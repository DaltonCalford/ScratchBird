#coding:utf-8

"""
ID:          isql-01
TITLE:       ISQL: check output of 'SHOW DATABASE' and 'SHOW DB'
DESCRIPTION: Check for correct output of SHOW DATABASE on empty database.
FBTEST:      functional.basic.isql.01
NOTES:
    [23.07.2025] pzotov
    Refactored: reduce code size by removing uneeded test* functions for each FB major version.
    Added check for 'SHOW DB;' command because of regression noted in #8659.
    Expected output must be identical for 'SHOW DATABASE' and 'SHOW DB' thus we can declare it
    as duplicated content of expected text for only 1st of these commands, see:
    act.expected_stdout = expected_stdout + '\n' + expected_stdout

    Checked on 6.0.0.1052; 5.0.3.1684; 4.0.6.3222; 3.0.13.33818.
"""

import pytest
from firebird.qa import *

db = db_factory()

# version: 3.0

substitutions = [ ('Owner.*', 'Owner'),
                  ('PAGE_SIZE.*', 'PAGE_SIZE'),
                  ('Number of DB pages allocated.*', 'Number of DB pages allocated'),
                  ('Number of DB pages used.*', 'Number of DB pages used'),
                  ('Number of DB pages free.*', 'Number of DB pages free'),
                  ('Sweep.*', 'Sweep'),
                  ('Forced Writes.*', 'Forced Writes'),
                  ('Transaction -.*', ''),
                  ('ODS.*', 'ODS'),
                  ('Wire crypt plugin.*', 'Wire crypt plugin'),
                  ('Creation date.*', 'Creation date'),
                  ('Protocol version.*', 'Protocol version'),
                  ('Default Character.*', 'Default Character')
                ]

test_script = """
    show database;
    show db; -- added for check #8659 (regression in 6.x)
"""

#substitutions = []
act = isql_act('db', test_script, substitutions=substitutions)

expected_stdout_3x = """
    Owner
    PAGE_SIZE
    Number of DB pages allocated
    Number of DB pages used
    Number of DB pages free
    Sweep
    Forced Writes
    ODS
    Database not encrypted
    Creation date
    Default Character
"""

expected_stdout_4x = """
    Owner
    PAGE_SIZE
    Number of DB pages allocated
    Number of DB pages used
    Number of DB pages free
    Sweep
    Forced Writes
    ODS
    Database not encrypted
    Wire crypt plugin
    Creation date
    Replica mode: NONE
    Protocol version
    Default Character
"""

expected_stdout_5x = """
    Owner
    PAGE_SIZE
    Number of DB pages allocated
    Number of DB pages used
    Number of DB pages free
    Sweep
    Forced Writes
    ODS
    Database not encrypted
    Wire crypt plugin
    Creation date
    Replica mode: NONE
    Protocol version
    Default Character
    Publication: Disabled
"""

expected_stdout_6x = """
    Owner
    PAGE_SIZE
    Number of DB pages allocated
    Number of DB pages used
    Number of DB pages free
    Sweep
    Forced Writes
    ODS
    Database not encrypted
    Wire crypt plugin
    Creation date
    Replica mode: NONE
    Protocol version
    Default Character
    Publication: Disabled
"""

@pytest.mark.version('>=3.0')
def test_1(act: Action):
    expected_stdout = expected_stdout_3x if act.is_version('<4') else expected_stdout_4x if act.is_version('<5') else expected_stdout_5x if act.is_version('<6') else expected_stdout_6x
    act.expected_stdout = expected_stdout + '\n' + expected_stdout
    act.execute(combine_output = True)
    assert act.clean_stdout == act.clean_expected_stdout
