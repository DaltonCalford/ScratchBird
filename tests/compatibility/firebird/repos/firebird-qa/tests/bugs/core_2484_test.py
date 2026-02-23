#coding:utf-8

"""
ID:          issue-2897
ISSUE:       https://github.com/FirebirdSQL/firebird/issues/2897
TITLE:       Success message when connecting to tiny trash database file
DESCRIPTION:
    We make invalid FDB file by creating binary file and write small string in it (text: 'ŒåŁä').
    Then we try to connect to such "database" using ISQL with passing trivial command
    like 'select current_timestamp' for execution.
    ISQL must raise error and quit (obviously without any result to STDOUT).

    STDERR differs dependign on OS.
    First line in error message is the same on Windows and Linux: "Statement failed, SQLSTATE = 08001",
    but starting from 2nd line messages differ:
    1) Windows:
      I/O error during "ReadFile" operation for file "..."
      -Error while trying to read from file
    2) Linux:
      I/O error during "read" operation for file "..."
      -File size is less than expected

    ::: NOTE ABOUT WINDOWS :::
    On Windows additional message did appear at last line, and it could be in localized form:
    -Overlapped I/O operation is in progress
    (only FB 4.0.x and 5.0.x were affected; NO such problem with FB 3.x)

    This has been considered as bug (see letter from Vlad, 16.09.2021 10:16, subject: "What to do with test for CORE-2484"),
    but if we want to check for presence of this message then we have to use codecs.open() invocation with suppressing
    with encoding = 'ascii' and suppressing non-writeable characters by specifying: errors = 'ignore'
    This bug was fixed long after time when this test was implemented:
      1) v4.0-release: fixed 19.09.2021 17:22, commit:
         https://github.com/FirebirdSQL/firebird/commit/54a2d5a39407b9d65b3f2b7ad614c3fc49abaa88
      2) refs/heads/master: fixed 19.09.2021 17:24, commit:
         https://github.com/FirebirdSQL/firebird/commit/90e1da6956f1c5c16a34d2704fafb92383212f37
JIRA:        CORE-2484
FBTEST:      bugs.core_2484
NOTES:
    Related issues:
        #6747 "Wrong message when connecting to tiny trash database file", ex. CORE-6518 // 18.03.2021
        #6755 "Connect to database that contains broken pages can lead to FB crash", ex. CORE-6528 // 31.03.2021
        #6968 "On Windows, engine may hung when works with corrupted database and read after the end of file" // 14.09.2021
        #8450  "Slightly better checking for a valid database file"
    [24.07.2025] pzotov
        Adjusted expected output in 6.x to actual, letter from dimitr 23.07.2025 11:35
        Checked on 6.0.0.1061; 5.0.3.1686; 4.0.6.3223; 3.0.13.33818
"""

import pytest
from firebird.qa import *
from pathlib import Path

substitutions = [  ('SQLSTATE = 08004', 'SQLSTATE = 08001'),
                   ('operation for file .*', 'operation for file'),
                   ('file .* is not a valid database', 'file is not a valid database'),
                ]

db = db_factory(charset='UTF8')

act = python_act('db', substitutions=substitutions)

tmp_fdb = temp_file('tmp_gh_2484_trash.tmp')

@pytest.mark.version('>=3.0')
def test_1(act: Action, tmp_fdb: Path):
    tmp_fdb.write_text( 'ŒåŁä', encoding='utf8' )

    if act.is_version('<6'):
        if act.platform == 'Windows':
            expected_stdout = """
                Statement failed, SQLSTATE = 08001
                I/O error during "ReadFile" operation for file
                -Error while trying to read from file
            """
        else:
            expected_stdout = """
                Statement failed, SQLSTATE = 08001
                I/O error during "read" operation for file
                -File size is less than expected
            """
    else:
        expected_stdout = """
            Statement failed, SQLSTATE = HY000
            file is not a valid database
        """

    act.expected_stdout = expected_stdout
    act.isql( switches=[ str(tmp_fdb), '-q' ], connect_db = False, input = 'select mon$database_name from mon$database;', combine_output = True )
    assert act.clean_stdout == act.clean_expected_stdout
