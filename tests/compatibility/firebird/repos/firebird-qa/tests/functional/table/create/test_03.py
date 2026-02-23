#coding:utf-8

"""
ID:          table.create-03
TITLE:       CREATE TABLE - charset + colations + domain
DESCRIPTION:
FBTEST:      functional.table.create.03
"""

import pytest
from firebird.qa import *

init_script = """CREATE DOMAIN test VARCHAR(32765)[40000];
commit;
"""

db = db_factory(init=init_script)

test_script = """CREATE TABLE test(
 c1 VARCHAR(40) CHARACTER SET CYRL COLLATE CYRL,
 c2 VARCHAR(40) CHARACTER SET DOS437 COLLATE DB_DEU437,
 c3 BLOB SUB_TYPE TEXT CHARACTER SET DOS437,
 c4 test
);
SHOW TABLE test;
"""

act = isql_act('db', test_script)

expected_stdout = """C1                              VARCHAR(40) CHARACTER SET CYRL Nullable
C2                              VARCHAR(40) CHARACTER SET DOS437 Nullable
                                 COLLATE DB_DEU437
C3                              BLOB segment 80, subtype TEXT CHARACTER SET DOS437 Nullable
C4                              (TEST) ARRAY OF [40000]
VARCHAR(32765) Nullable
"""

@pytest.mark.skip("Test will be reimplemented.")
@pytest.mark.version('>=3.0')
def test_1(act: Action):
    act.expected_stdout = expected_stdout
    act.execute()
    assert act.clean_stdout == act.clean_expected_stdout
