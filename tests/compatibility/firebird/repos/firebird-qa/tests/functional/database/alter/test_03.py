#coding:utf-8

"""
ID:          alter-database-03
TITLE:       Alter database: add file with name of this database or previously added files must fail
DESCRIPTION: Add same file twice must fail
NOTES:
    [08.02.2022] pcisar
    Fails on Windows with 3.0.8:
    Regex pattern '.*Cannot add file with the same name as the database or added files.*'
    does not match 'unsuccessful metadata update\n-ALTER DATABASE failed\n-unknown ISC error 336068774'.

    [08.04.2022] pzotov
    Test PASSES on FB 3.0.8 Rls, 4.0.1 RLs and 5.0.0.467.

    [29.12.2024] pzotov
    Added restriction for FB 6.x: this test now must be skipped, see:
    https://github.com/FirebirdSQL/firebird/commit/f0740d2a3282ed92a87b8e0547139ba8efe61173
    ("Wipe out multi-file database support (#8047)")
"""

import pytest
import platform
from firebird.qa import *
from firebird.driver import DatabaseError

db = db_factory()

act = python_act('db')

@pytest.mark.version('>=3.0,<6')
def test_1(act: Action, capsys):
    with act.db.connect() as con:
        with con.cursor() as c:
            c.execute(f"ALTER DATABASE ADD FILE '{act.db.db_path.with_name('TEST.G00')}' STARTING 10000")
            con.commit()
            with pytest.raises(DatabaseError, match='.*Cannot add file with the same name as the database or added files.*'):
                c.execute(f"ALTER DATABASE ADD FILE '{act.db.db_path.with_name('TEST.G00')}' STARTING 20000")
    # Passed
