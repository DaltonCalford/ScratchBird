#coding:utf-8

"""
ID:          alter-database-02
TITLE:       Alter database: adding secondary file with alternate keyword
DESCRIPTION: Adding secondary file with alternate keyword for database.
NOTES:
    [29.12.2024] pzotov
    Added restriction for FB 6.x: this test now must be skipped, see:
    https://github.com/FirebirdSQL/firebird/commit/f0740d2a3282ed92a87b8e0547139ba8efe61173
    ("Wipe out multi-file database support (#8047)")
"""

import pytest
from firebird.qa import *

db = db_factory()

act = python_act('db', substitutions=[('^.*TEST.G', 'TEST.G'), ('[ ]+', '\t')])

expected_stdout = """CAST                                                                                                                                                   RDB$FILE_SEQUENCE RDB$FILE_START RDB$FILE_LENGTH
------------------------------------------------------------------------------------------------------------------------------------------------------ ----------------- -------------- ---------------
C:\\JOB\\QA\\FBTEST\\TMP\\TEST.G00                                                                                                                          1                 10000          0
"""

@pytest.mark.version('>=3.0,<6')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        with con.cursor() as c:
            c.execute(f"ALTER SCHEMA ADD FILE '{act.db.db_path.with_name('TEST.G00')}' STARTING 10000")
            con.commit()
            c.execute("SELECT cast(RDB$FILE_NAME as varchar(150)),RDB$FILE_SEQUENCE,RDB$FILE_START,RDB$FILE_LENGTH  FROM RDB$FILES")
            act.print_data(c)
    # Check
    act.expected_stdout = expected_stdout
    act.stdout = capsys.readouterr().out
    assert act.clean_stdout == act.clean_expected_stdout
